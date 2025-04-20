/*
 * This file is part of EasyRPG Player.
 *
 * EasyRPG Player is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
 */

#include "web_api.h"
#include <cstdlib>
#include <fstream>
#include <map>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#ifdef EMSCRIPTEN
#  include <emscripten.h>
#  include <lcf/reader_util.h>
#elif defined(PLAYER_YNO)
#  include <lcf/reader_util.h>
#  include <cpr/cpr.h>
#  include <uv.h>
#  include <mutex>
#  include "platform.h"
#  include "multiplayer/game_multiplayer.h"
#  include "player.h"
#  define EP_CONTAINER_OF(ptr, type, member) (type*)((char*)ptr - offsetof(type, member))
#  if defined(_WIN32)
#    define timegm _mkgmtime
#  endif
#endif

#include "async_handler.h"
#include "cache.h"
#include "filefinder.h"
#include "memory_management.h"
#include "output.h"
#include "player.h"
#include "main_data.h"
#include "utils.h"
#include "transition.h"
#include "rand.h"

// When this option is enabled async requests are randomly delayed.
// This allows testing some aspects of async file fetching locally.
//#define EP_DEBUG_SIMULATE_ASYNC

namespace {
	std::unordered_map<std::string, std::shared_ptr<FileRequestAsync>> async_requests;
	std::unordered_map<std::string, std::string> file_mapping;
	int next_id = 0;
	int index_version = 1;
	int64_t db_lastwrite = LLONG_MAX;

	std::vector<std::unique_ptr<cpr::Session>> session_pool;
	std::mutex session_mutex{};

	std::unique_ptr<cpr::Session> AcquireSession() {
		std::lock_guard _guard(session_mutex);
		if (session_pool.empty())
			return std::make_unique<cpr::Session>();
		std::unique_ptr<cpr::Session> out;
		out.swap(session_pool.back());
		session_pool.pop_back();
		return out;
	}
	void ReleaseSession(std::unique_ptr<cpr::Session>&& session) {
		std::lock_guard _guard(session_mutex);
		session_pool.push_back(std::move(session));
	}

	FileRequestAsync* GetRequest(const std::string& path) {
		auto it = async_requests.find(path);

		if (it != async_requests.end()) {
			return &*(it->second);
		}
		return nullptr;
	}

	FileRequestAsync* RegisterRequest(std::string path, std::string directory, std::string file)
	{
		auto p = async_requests.emplace(
			std::move(path),
			std::make_shared<FileRequestAsync>(path, std::move(directory), std::move(file))
		);
		return &*(p.first->second);
	}

	FileRequestBinding CreatePending() {
		return std::make_shared<int>(next_id++);
	}

	constexpr size_t ASYNC_MAX_RETRY_COUNT{ 16 };

	struct async_download_context {
		std::string url, file, param;
		std::weak_ptr<FileRequestAsync> obj;
		size_t count;
#ifndef EMSCRIPTEN
		uv_work_t uvctx;
		int http_status = 500;
#endif

		async_download_context(
			std::string u,
			std::string f,
			std::string p,
			FileRequestAsync* o
		) : url{ std::move(u) }, file{ std::move(f) }, param{ std::move(p) }, obj{ o->weak_from_this() }, count{}
#ifndef EMSCRIPTEN
			, uvctx{}
#endif
		{}
	};

	void download_success_retry(unsigned, void* userData, const char*) {
		auto ctx = static_cast<async_download_context*>(userData);
		auto sobj = ctx->obj.lock();
		// in case that object got deleted
		if (sobj)
			sobj->DownloadDone(true);
		delete ctx;
	}

	void start_async_wget_with_retry(async_download_context* ctx);

	void download_failure_retry(unsigned, void* userData, int status) {
		auto ctx = static_cast<async_download_context*>(userData);
		++ctx->count;
		bool flag1 = ctx->count >= ASYNC_MAX_RETRY_COUNT;
		bool flag2 = status >= 400;
		auto sobj = ctx->obj.lock();
		std::string_view download_path{ (sobj) ? sobj->GetPath() : "(deleted)" };
		if (flag1 || flag2) {
			if (flag1)
				Output::Warning("DL Failure: max retries exceeded: {}", download_path);
			else if (flag2)
				Output::Warning("DL Failure: file not available: {} ({})", download_path, status);

			if (sobj)
				sobj->DownloadDone(false);
			delete ctx;
			return;
		}
		Output::Debug("DL Failure: {}. Retrying", download_path);
		start_async_wget_with_retry(ctx);
	}


	void start_async_wget_with_retry(async_download_context* ctx) {
#ifdef EMSCRIPTEN
		emscripten_async_wget2(
			ctx->url.data(),
			ctx->file.data(),
			"GET",
			ctx->param.data(),
			ctx,
			download_success_retry,
			download_failure_retry,
			nullptr
		);
#else
		uv_queue_work(uv_default_loop(), &ctx->uvctx,
		[](uv_work_t *task) {
			auto ctx = EP_CONTAINER_OF(task, async_download_context, uvctx);
			auto sobj = ctx->obj.lock();
			if (sobj) {
				std::string url_(ctx->url);
				std::string path_(ctx->file);
				if (path_.empty())
					path_ = sobj->GetPath();
				if (!sobj->GetRequestExtension().empty()) {
					url_ += sobj->GetRequestExtension();
					path_ += sobj->GetRequestExtension();
				}

				Platform::File handle(FileFinder::MakeCanonical(fmt::format("{}/..", path_), 0));
				handle.MakeDirectory(true);

				std::ofstream out(std::filesystem::u8path(path_), std::ios::binary);

				auto session = AcquireSession();
				session->SetUrl(cpr::Url{ url_ });
				auto resp = session->Download(out);
				out.flush();
				ReleaseSession(std::move(session));

				ctx->http_status = resp.status_code;
				if (resp.status_code == 0) {
					ctx->http_status = 1000 + (int)resp.error.code;
					Output::Debug("failed {}: {}", resp.error.message);
				}
				if (ctx->file == "RPG_RT.ldb") {
					// use this file to determine cache freshness
					if (auto lm = resp.header.find("last-modified"); lm != resp.header.end()) {
						std::tm time{};
						std::istringstream ss(lm->second);
						ss >> std::get_time(&time, "%a, %d %b %Y %H:%M:%S");
						if (ss.fail())
							Output::Debug("could not parse Last-Modified: {}", lm->second);
						else {
							Output::Debug("ldb last-modified: {}", lm->second);
							db_lastwrite = timegm(&time);
						}
					}
				}
			}
		},
		[](uv_work_t *task, int status) {
			auto ctx = EP_CONTAINER_OF(task, async_download_context, uvctx);
			if (status) {
				Output::Debug("Task cancelled ({})", status);
				return download_failure_retry(0, ctx, 500);
			}
			if (ctx->http_status >= 200 && ctx->http_status < 300)
				download_success_retry(0, ctx, "");
			else
				download_failure_retry(0, ctx, ctx->http_status);
		});
#endif
	}
	void async_wget_with_retry(
		std::string url,
		std::string file,
		std::string param,
		FileRequestAsync* obj
	) {
		// ctx will be deleted when download succeeds
		auto ctx = new async_download_context{ url, file, param, obj };
		start_async_wget_with_retry(ctx);
	}
}

void AsyncHandler::CreateRequestMapping(const std::string& file) {
	auto f = FileFinder::Game().OpenInputStream(file);
	if (!f) {
		Output::Error("Reading index.json failed");
		return;
	}

	json j = json::parse(f, nullptr, false);
	if (j.is_discarded()) {
		Output::Error("index.json is not a valid JSON file");
		return;
	}

	if (j.contains("metadata") && j["metadata"].is_object()) {
		const auto& metadata = j["metadata"];
		if (metadata.contains("version") && metadata["version"].is_number()) {
			index_version = metadata["version"].get<int>();
		}
	}

	Output::Debug("Parsing index.json version {}", index_version);

	if (index_version <= 1) {
		// legacy format
		for (const auto& value : j.items()) {
			file_mapping[value.key()] = value.value().get<std::string>();
		}
	} else {
		using fn = std::function<void(const json&, const std::string&)>;
		fn parse = [&] (const json& obj, const std::string& path) {
			std::string dirname;
			if (obj.contains("_dirname") && obj["_dirname"].is_string()) {
				dirname = obj["_dirname"].get<std::string>();
			}
			dirname = FileFinder::MakePath(path, dirname);

			for (const auto& value : obj.items()) {
				const auto& second = value.value();
				if (second.is_object()) {
					parse(second, dirname);
				} else if (second.is_string()){
					file_mapping[FileFinder::MakePath(Utils::LowerCase(dirname), value.key())] = FileFinder::MakePath(dirname, second.get<std::string>());
				}
			}
		};

		if (j.contains("cache") && j["cache"].is_object()) {
			parse(j["cache"], "");
		}

		// Create some empty DLL files. Engine & patch detection depend on them.
		for (const auto& s : {"harmony.dll", "ultimate_rt_eb.dll", "dynloader.dll", "accord.dll"}) {
			auto it = file_mapping.find(s);
			if (it != file_mapping.end()) {
				FileFinder::Game().OpenOutputStream(s);
			}
		}

		// Look for Meta.ini files and fetch them. They are required for detecting the translations.
		for (const auto& item: file_mapping) {
			if (EndsWith(item.first, "meta.ini")) {
				auto* request = AsyncHandler::RequestFile(item.second);
				request->SetImportantFile(true);
				request->Start();
			}
		}
	}
}

void AsyncHandler::ClearRequests() {
	auto it = async_requests.begin();
	while (it != async_requests.end()) {
		if (it->second->IsReady()) {
			it = async_requests.erase(it);
		} else {
			++it;
		}
	}
	async_requests.clear();
	db_lastwrite = LLONG_MAX;
}

FileRequestAsync* AsyncHandler::RequestFile(std::string_view folder_name, std::string_view file_name) {
	auto path = FileFinder::MakePath(folder_name, file_name);

	auto* request = GetRequest(path);

	if (request) {
		return request;
	}

	Web_API::OnRequestFile(path);

	return RegisterRequest(std::move(path), std::string(folder_name), std::string(file_name));
}

FileRequestAsync* AsyncHandler::RequestFile(std::string_view file_name) {
	return RequestFile(".", file_name);
}

bool AsyncHandler::IsFilePending(bool important, bool graphic) {
	for (auto& ap: async_requests) {
		FileRequestAsync& request = *ap.second;

#ifdef EP_DEBUG_SIMULATE_ASYNC
		request.UpdateProgress();
#endif

		if (!request.IsReady()
				&& (!important || request.IsImportantFile())
				&& (!graphic || request.IsGraphicFile())
				) {
			return true;
		}
	}

	return false;
}

void AsyncHandler::SaveFilesystem() {
#ifdef EMSCRIPTEN
	// Save changed file system
	EM_ASM({
		FS.syncfs(function(err) {});
	});
#endif
}

void AsyncHandler::SaveFilesystem(int slot_id) {
#ifdef EMSCRIPTEN
	// Save changed file system
	EM_ASM({
		FS.syncfs(function(err) {
			onSaveSlotUpdated($0);
		});
	}, slot_id);
#elif defined(PLAYER_YNO)
	if (slot_id != 1) return;
	GMI().UploadSaveFile();
#endif
}

bool AsyncHandler::IsImportantFilePending() {
	return IsFilePending(true, false);
}

bool AsyncHandler::IsGraphicFilePending() {
	return IsFilePending(false, true);
}

FileRequestAsync::FileRequestAsync(std::string path, std::string directory, std::string file) :
	directory(std::move(directory)),
	file(std::move(file)),
	path(std::move(path)),
	state(State_WaitForStart)
{
}

void FileRequestAsync::SetGraphicFile(bool graphic) {
	this->graphic = graphic;
	// We need this flag in order to prevent show screen transitions
	// from starting util all graphical assets are loaded.
	// Also, the screen is erased, so you can't see any delays :)
	if (Transition::instance().IsErasedNotActive()) {
		SetImportantFile(true);
	}
}

void FileRequestAsync::Start() {
	if (file == CACHE_DEFAULT_BITMAP) {
		// Embedded asset -> Fire immediately
		DownloadDone(true);
		return;
	}

	if (state == State_Pending) {
		return;
	}

	if (IsReady()) {
		// Fire immediately
		DownloadDone(true);
		return;
	}

	state = State_Pending;

#if defined(EMSCRIPTEN) || defined(PLAYER_YNO)
	std::string request_path;
#  ifdef EM_GAME_URL
	request_path = EM_GAME_URL;
#  else
	if (parent_scope)
		request_path = "https://ynoproject.net/";
	else
		request_path = "https://ynoproject.net/data/";
#  endif

	if (!Player::emscripten_game_name.empty()) {
		request_path += Player::emscripten_game_name + "/";
	} else {
		request_path += "2kki/";
	}

	std::string modified_path;
	if (index_version >= 2) {
		modified_path = lcf::ReaderUtil::Normalize(path);
		modified_path = FileFinder::MakeCanonical(modified_path, 1);
	} else {
		modified_path = Utils::LowerCase(path);
		if (directory != ".") {
			modified_path = FileFinder::MakeCanonical(modified_path, 1);
		} else {
			auto it = file_mapping.find(modified_path);
			if (it == file_mapping.end()) {
				modified_path = FileFinder::MakeCanonical(modified_path, 1);
			}
		}
	}

	if (graphic && Tr::HasActiveTranslation()) {
		std::string modified_path_trans = FileFinder::MakePath(lcf::ReaderUtil::Normalize(Tr::GetCurrentTranslationFilesystem().GetFullPath()), modified_path);
		auto it = file_mapping.find(modified_path_trans);
		if (it != file_mapping.end()) {
			modified_path = modified_path_trans;
		}
	}


	auto it = file_mapping.find(modified_path);
	if (it != file_mapping.end()) {
		request_path += it->second;
	} else {
		if (file_mapping.empty() || parent_scope) {
			// index.json not fetched yet, fallthrough and fetch
			std::string path_(this->path);
			size_t offset;
			if (parent_scope && (offset = path_.find("../")) != std::string::npos)
				path_ = path_.substr(offset + 3);
			request_path += path_;
		} else {
			// Fire immediately (error)
			Output::Debug("{} not in index.json", modified_path);
			DownloadDone(false);
			return;
		}
	}

	// URL encode %, # and +
	request_path = Utils::ReplaceAll(request_path, "%", "%25");
	request_path = Utils::ReplaceAll(request_path, "#", "%23");
	request_path = Utils::ReplaceAll(request_path, "+", "%2B");
	request_path = Utils::ReplaceAll(request_path, " ", "%20");

	std::string request_file = (it != file_mapping.end() ? it->second : path);

#ifndef EMSCRIPTEN
	if (Platform::File(request_path).GetLastModified() >= db_lastwrite) {
		DownloadDone(true);
		return;
	}
#endif

	async_wget_with_retry(request_path, std::move(request_file), "", this);
#else
#  ifdef EM_GAME_URL
#    warning EM_GAME_URL set and not an Emscripten build!
#  endif

#  ifndef EP_DEBUG_SIMULATE_ASYNC
	DownloadDone(true);
#  endif
#endif
}

void FileRequestAsync::UpdateProgress() {
#ifndef EMSCRIPTEN
	// Fake download for testing event handlers

	if (!IsReady() && Rand::ChanceOf(1, 100)) {
		DownloadDone(true);
	}
#endif
}

FileRequestBinding FileRequestAsync::Bind(void(*func)(FileRequestResult*)) {
	FileRequestBinding pending = CreatePending();

	listeners.emplace_back(FileRequestBindingWeak(pending), func);

	return pending;
}

FileRequestBinding FileRequestAsync::Bind(std::function<void(FileRequestResult*)> func) {
	FileRequestBinding pending = CreatePending();

	listeners.emplace_back(FileRequestBindingWeak(pending), func);

	return pending;
}

void FileRequestAsync::CallListeners(bool success) {
	FileRequestResult result { directory, file, -1, success };

	for (auto& listener : listeners) {
		if (!listener.first.expired()) {
			result.request_id = *listener.first.lock();
			(listener.second)(&result);
		} else {
			Output::Debug("Request cancelled: {}", GetPath());
		}
	}

	listeners.clear();
}

void FileRequestAsync::DownloadDone(bool success) {
	if (IsReady()) {
		// Change to real success state when already finished before
		success = state == State_DoneSuccess;
	}

	if (success) {
		if (state == State_Pending) {
			// Update directory structure (new file was added)
			if (FileFinder::Game()) {
				FileFinder::Game().ClearCache();
			}
		}

		state = State_DoneSuccess;

		CallListeners(true);
	}
	else {
		state = State_DoneFailure;

		CallListeners(false);
	}
}
