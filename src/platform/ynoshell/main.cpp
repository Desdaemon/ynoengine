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

class YnoApp : public wxApp
{
public:
	bool OnInit() override;
};

wxIMPLEMENT_APP(YnoApp);

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
	wxTextCtrl* chatInput;

private:
	void OnKeyDownFrame(wxKeyEvent& event);
};


bool YnoApp::OnInit()
{
	YnoFrame* frame = new YnoFrame();
	frame->Show(true);

	wxWindow* child = frame->gameWindow;
	if (!child) {
		Output::Error("no child frame");
		return false;
	}

	Player::did_parse_config = [handle=child->GetHandle()](Game_Config& cfg) {
		cfg.video.foreign_window_handle = (void*)handle;
	};

	auto& app = wxGetApp();
	std::vector<std::string> args{ (char**)app.argv, (char**)app.argv + app.argc };
	Player::Init(std::move(args));
	Player::Run();
}

YnoFrame::YnoFrame()
	: wxFrame(nullptr, wxID_ANY, "YNOproject")
{
	wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

	gameWindow = new YnoGameContainer(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE);
	gameWindow->Bind(wxEVT_KEY_DOWN, &YnoFrame::OnKeyDownFrame, this);
	sizer->Add((wxWindow*)gameWindow, 1, wxEXPAND);

	GMI().on_chat_msg = [](std::string_view msg, std::string_view sender, std::string_view system, bool) {
		Graphics::GetChatOverlay().AddMessage(msg, sender, system);
	};

	SetSizer(sizer);
};

void YnoFrame::OnKeyDownFrame(wxKeyEvent& event) {
	switch (event.GetKeyCode()) {
	case WXK_RETURN:
		if (event.AltDown())
			ShowFullScreen(!IsFullScreen(), wxFULLSCREEN_ALL);
		return;
	}
	event.Skip();
}

