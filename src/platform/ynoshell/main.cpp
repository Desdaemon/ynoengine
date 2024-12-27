// Start of wxWidgets "Hello World" Program
#include <wx/wx.h>
#include <wx/custombgwin.h>
#include <wx/url.h>

#include "output.h"
#include "player.h"
#include "cache.h"
#include "bitmap.h"
#include "graphics.h"
#include "multiplayer/chat_overlay.h"
#include "multiplayer/game_multiplayer.h"

class YnoFrame;

class YnoApp : public wxApp
{
public:
	bool OnInit() override;
	YnoFrame* frame;
};

//wxIMPLEMENT_APP(YnoApp);
wxIMPLEMENT_APP_NO_MAIN(YnoApp);

//class YnoChatMsg : public wxPanel
//{
//public:
//	wxSizer* parentSizer;
//	wxSizerItem* selfItem;
//	std::string msg_;
//	wxString msgref;
//	wxStaticText* text;
//	YnoChatMsg(wxBoxSizer* parentSizer, wxWindow* parent, std::string msg)
//		: wxPanel(parent, wxID_ANY),
//		msg_(std::move(msg)),
//		msgref(wxString(msg_)),
//		parentSizer(parentSizer),
//		selfItem(parentSizer->Add(this, 0, wxALL, Zoom(6)))
//	{
//		auto sizer = new wxBoxSizer(wxVERTICAL);
//		text = new wxStaticText(this, wxID_ANY, msgref, wxDefaultPosition);
//		text->SetFont(GetFont().Scaled(zoom));
//		text->Wrap(Zoom(parentSizer->GetSize().GetWidth() - 24));
//		sizer->Add(text);
//		SetSizer(sizer);
//	}
//
//	bool Destroy() override {
//		parentSizer->Remove((wxSizer*)selfItem);
//		return wxPanel::Destroy();
//	}
//
//	void onPaint(wxPaintEvent&) {
//
//	}
//};

using YnoGameContainer = wxWindow;

class YnoFrame : public wxFrame
{
public:
	YnoFrame();

	YnoGameContainer* gameWindow;
	//wxTextCtrl* chatInput;

private:
	void OnKeyDownFrame(wxKeyEvent& event);
	void OnFocus(wxActivateEvent& event);
};


bool YnoApp::OnInit()
{
	frame = new YnoFrame();
	frame->Show(true);

	YnoGameContainer* child = frame->gameWindow;
	if (!child) {
		Output::Error("no child frame");
		return false;
	}

	Player::did_parse_config = [handle=child->GetHWND()](Game_Config& cfg) {
		cfg.video.foreign_window_handle = (void*)handle;
	};

	auto& app = wxGetApp();
	std::vector<std::string> args{ (char**)app.argv, (char**)app.argv + app.argc };
	Player::Init(std::move(args));
	Player::Run();

	exit(Player::exit_code);
}

YnoFrame::YnoFrame()
	: wxFrame(nullptr, wxID_ANY, "YNOproject")
{
	wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

	gameWindow = new YnoGameContainer(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS);
	gameWindow->Bind(wxEVT_KEY_DOWN, &YnoFrame::OnKeyDownFrame, this);
	sizer->Add((wxWindow*)gameWindow, 1, wxEXPAND);

	GMI().on_chat_msg = [](Game_Multiplayer::ChatMsg msg) {
		Graphics::GetChatOverlay().AddMessage(msg.content, msg.sender, msg.system, msg.badge, msg.account);
	};

	Bind(wxEVT_ACTIVATE, &YnoFrame::OnFocus, this);

	SetSizer(sizer);
};

void YnoFrame::OnFocus(wxActivateEvent& event) {
	event.Skip();
}

void YnoFrame::OnKeyDownFrame(wxKeyEvent& event) {
	switch (event.GetKeyCode()) {
	case WXK_RETURN:
		if (event.AltDown())
			ShowFullScreen(!IsFullScreen(), wxFULLSCREEN_ALL);
		return;
	}
	event.Skip();
}

int main(int argc, char** argv) {
	Output::SetLogLevel(LogLevel::Debug);
	wxEntry(argc, argv);
}
