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

//class YnoGameContainer : public wxCustomBackgroundWindow<wxWindow>
//{
//public:
//	YnoGameContainer(wxWindow* parent) {
//		Create(parent, wxID_ANY);
//	};
//};
using YnoGameContainer = wxWindow;

class YnoFrame : public wxFrame
{
public:
	YnoFrame();

	YnoGameContainer* gameWindow;
	//wxScrolledWindow* chatLog;
	//wxBoxSizer* chatboxSizer;
	//wxBoxSizer* chatLogSizer;
	//wxTextCtrl* chatInput;
	//bool shouldScrollChat = true;
	//std::vector<YnoChatMsg*> chatmsgs;

private:
	//void DoZoom(bool magnify);
	void OnKeyDownFrame(wxKeyEvent& event);
	//void OnKeyDownChatInput(wxKeyEvent& event);
	//void OnScrollChatLog(wxScrollWinEvent& event);
	//void OverlayPaint(wxPaintEvent&);
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

	return true;
}

YnoFrame::YnoFrame()
	: wxFrame(nullptr, wxID_ANY, "YNOproject")
{
	//SetFont({ 16, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL });
	wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

	//childPanel = new wxWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE);

	gameWindow = new YnoGameContainer(this, wxID_ANY);
	gameWindow->Create(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE);
	gameWindow->Bind(wxEVT_KEY_DOWN, &YnoFrame::OnKeyDownFrame, this);
	sizer->Add((wxWindow*)gameWindow, 1, wxEXPAND);

	GMI().on_chat_msg = [](std::string_view msg, bool) {
		Graphics::GetChatOverlay().AddMessage(msg.data());
	};


	//childPanel->SetBackgroundColour(*wxBLACK);
	//childPanel->Bind(wxEVT_KEY_DOWN, &YnoFrame::OnKeyDownFrame, this);
	//sizer->Add(childPanel, 1, wxEXPAND);



	//chatboxSizer = new wxBoxSizer(wxVERTICAL);

	//auto chatwindow = new wxRpgWindow(this);
	//auto chatwindowSizer = new wxBoxSizer(wxHORIZONTAL);
	//chatwindowSizer->Add(chatboxSizer, 1, wxEXPAND);
	//chatwindow->SetSizer(chatwindowSizer);

	//sizer->Add(chatwindowSizer, 0, wxEXPAND | wxDOWN);

	//chatLog = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxSize(Zoom(350), -1), wxVSCROLL);
	//chatLog->SetBackgroundColour(wxTRANSPARENT);
	//chatLog->SetScrollRate(0, Zoom(25));
	//chatLogSizer = new wxBoxSizer(wxVERTICAL);
	//chatLog->SetSizer(chatLogSizer);
	//chatLog->Bind(wxEVT_SCROLLWIN_LINEUP, &YnoFrame::OnScrollChatLog, this);
	//chatLog->Bind(wxEVT_SCROLLWIN_LINEDOWN, &YnoFrame::OnScrollChatLog, this);
	//chatboxSizer->Add(chatLog, 1, wxEXPAND);

	//GMI().on_chat_msg = [this](std::string_view msg, bool syncing) {
	//	if (chatmsgs.size() > 100) {
	//		chatmsgs[0]->Destroy();
	//		chatmsgs.erase(chatmsgs.begin());
	//	}
	//	chatmsgs.emplace_back(new YnoChatMsg(chatLogSizer, chatLog, std::string(msg)));
	//	if (syncing) return;
	//	chatboxSizer->Layout();
	//	Refresh();
	//	if (shouldScrollChat)
	//		chatLog->Scroll(wxPoint(-1, chatLog->GetScrollRange(wxVSCROLL)));
	//};

	//chatInput = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(-1, Zoom(50)));
	//chatInput->Bind(wxEVT_KEY_DOWN, &YnoFrame::OnKeyDownChatInput, this);
	//chatInput->SetFont(GetFont().Scaled(zoom));

	//auto chatInputSizer = new wxBoxSizer(wxHORIZONTAL);
	//chatInputSizer->Add(chatInput, 1, wxEXPAND | wxALL);
	//chatboxSizer->Add(chatInputSizer);

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
//
//void YnoFrame::OnKeyDownChatInput(wxKeyEvent& event) {
//	switch (event.GetKeyCode()) {
//	case WXK_TAB:
//		childPanel->SetFocus();
//		break;
//	case WXK_RETURN:
//		chatInput->Clear();
//		break;
//	default:
//		event.Skip();
//	}
//}
//
//void YnoFrame::OnScrollChatLog(wxScrollWinEvent& event) {
//	int diff = chatLog->GetScrollRange(wxVERTICAL) - chatLog->GetScrollPos(wxVERTICAL);
//	shouldScrollChat = diff < Zoom(55);
//	event.Skip();
//}
//
//void YnoFrame::DoZoom(bool magnify) {
//	float step = zoom <= 1 ? 0.1 : 0.25;
//	float wanted = magnify ? zoom + step : zoom - step;
//	zoom = std::clamp(wanted, 0.8f, 2.f);
//
//	int oldPosition = chatLog->GetScrollPos(wxVERTICAL);
//	chatboxSizer->SetMinSize(wxSize(Zoom(350), -1));
//	chatInput->SetMinSize(wxSize(-1, Zoom(50)));
//	for (auto msg : chatmsgs) {
//		msg->text->SetFont(msg->GetFont().Scaled(zoom));
//	}
//	Layout();
//	Refresh();
//	chatLog->Scroll(wxPoint(-1, Zoom(oldPosition)));
//}
