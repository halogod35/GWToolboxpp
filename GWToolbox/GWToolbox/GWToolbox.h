#pragma once

#include <Windows.h>

#include <GWCA\GWCA.h>
#include <GWCA\ChatMgr.h>

#include <OSHGui\OSHGui.hpp>
#include <OSHGui\Input\WindowsMessage.hpp>

#include "ChatLogger.h"
#include "Config.h"
#include "ChatCommands.h"

#include "MainWindow.h"
#include "TimerWindow.h"
#include "BondsWindow.h"
#include "HealthWindow.h"
#include "DistanceWindow.h"
#include "PartyDamage.h"


#ifdef _DEBUG
#define EXCEPT_EXPRESSION_ENTRY EXCEPTION_CONTINUE_SEARCH
//#define EXCEPT_EXPRESSION_LOOP EXCEPTION_CONTINUE_SEARCH
#define EXCEPT_EXPRESSION_LOOP EXCEPTION_EXECUTE_HANDLER
#else
#define EXCEPT_EXPRESSION_ENTRY Logger::GenerateDump(GetExceptionInformation())
#define EXCEPT_EXPRESSION_LOOP EXCEPTION_EXECUTE_HANDLER
#endif

class GWToolbox {
public:
	static const wchar_t * Host;
	static const wchar_t* Version;

	//------ Static Fields ------//
private:
	static GWToolbox* instance_;
	static OSHGui::Drawing::Direct3D9Renderer* renderer;
	static long OldWndProc;
	static OSHGui::Input::WindowsMessage input;

	//------ Static Methods ------//
public:

	// will create a new toolbox object and run it, can be used as argument for createThread
	static void SafeThreadEntry(HMODULE mod);
private:
	static void ThreadEntry(HMODULE dllmodule);

	static void SafeCreateGui(IDirect3DDevice9* pDevice);
	static void CreateGui(IDirect3DDevice9* pDevice);

	// DirectX event handlers declaration
	static HRESULT WINAPI endScene(IDirect3DDevice9* pDevice);
	static HRESULT WINAPI resetScene(IDirect3DDevice9* pDevice,
		D3DPRESENT_PARAMETERS* pPresentationParameters);

	// Input event handler
	static LRESULT CALLBACK SafeWndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);


	//------ Private Fields ------//
private:
	HMODULE dll_module_;	// Handle to the dll module we are running, used to clear the module from GW on eject.

	bool initialized_;
	bool capture_input_;
	bool must_self_destruct_;

	Config* config_;
	ChatCommands* chat_commands_;

	MainWindow* main_window_;
	TimerWindow* timer_window_;
	BondsWindow* bonds_window_;
	HealthWindow* health_window_;
	DistanceWindow* distance_window_;
	PartyDamage* party_damage_;

	//------ Constructor ------//
private:
	GWToolbox(HMODULE mod) :
		dll_module_(mod),
		config_(new Config()),
		chat_commands_(new ChatCommands()),
		main_window_(nullptr),
		timer_window_(nullptr),
		bonds_window_(nullptr),
		health_window_(nullptr),
		distance_window_(nullptr),
		party_damage_(nullptr),
		initialized_(false),
		must_self_destruct_(false),
		capture_input_(false) {

		GWCA::Api::Chat().RegisterChannel(L"GWToolbox++", 0x00CCFF, 0xDDDDDD);
		GWCA::Api::Chat().SetTimestampColor(0xBBBBBB);
		GWCA::Api::Chat().ToggleTimeStamp(config().IniReadBool(
			MainWindow::IniSection(), MainWindow::IniKeyTimestamps(), true));

		if (GWCA::Api::Map().GetInstanceType() != GwConstants::InstanceType::Loading) {
			DWORD playerNumber = GWCA::Api::Agents().GetPlayer()->PlayerNumber;
			ChatLogger::LogF(L"Hello %ls!", GWCA::Api::Agents().GetPlayerNameByLoginNumber(playerNumber));
		}
	}

	//------ Private Methods ------//
private:
	// Does everything: setup, main loop, destruction 
	void Exec();
	void UpdateUI();

	void LoadTheme();
	void SaveTheme();

	//------ Setters ------//
private:
	inline void set_initialized() { initialized_ = true; }
	inline void set_main_window(MainWindow* w) { main_window_ = w; }
	inline void set_timer_window(TimerWindow* w) { timer_window_ = w; }
	inline void set_bonds_window(BondsWindow* w) { bonds_window_ = w; }
	inline void set_health_window(HealthWindow* w) { health_window_ = w; }
	inline void set_distance_window(DistanceWindow* w) { distance_window_ = w; }
	inline void set_party_damage(PartyDamage* w) { party_damage_ = w; }

	//------ Public methods ------//
public:
	static GWToolbox& instance() { return *instance_; }

	inline bool initialized() { return initialized_; }

	inline bool capture_input() { return capture_input_; }
	inline void set_capture_input(bool capture) { capture_input_ = capture; }
	
	inline Config& config() { return *config_; }
	inline ChatCommands& chat_commands() { return *chat_commands_; }

	inline MainWindow& main_window() { return *main_window_; }
	inline TimerWindow& timer_window() { return *timer_window_; }
	inline BondsWindow& bonds_window() { return *bonds_window_; }
	inline HealthWindow& health_window() { return *health_window_; }
	inline DistanceWindow& distance_window() { return *distance_window_; }
	inline PartyDamage& party_damage() { return *party_damage_; }
	
	void StartSelfDestruct() { 
		if (GWCA::Api::Map().GetInstanceType() != GwConstants::InstanceType::Loading) {
			ChatLogger::Log(L"Bye!");
		}
		must_self_destruct_ = true; }
};
