#include "script.h"
#include "keyboard.h"
#include "../../GTAVManualTransmission/Gears/Memory/VehicleExtensions.hpp"
#include "../../GTAVManualTransmission/Gears/Util/Util.hpp"
#include "../../GTAVManualTransmission/Gears/Util/simpleini/SimpleIni.h"
#include "controls.h"
#include "presets.h"
#include "settings.h"
#include "MenuClass.h"

Menu menu;

const std::string settingsFolder = "./VStancer/";
const std::string settingsFile = settingsFolder+"Settings.ini";
const std::string savedCarsFile = settingsFolder+"SavedCars.xml";
const std::string presetsFile = settingsFolder +"Presets.xml";
LPCWSTR menuStyleLocation = L".\\VStancer\\MenuStyle.ini";

std::string cancelText = "wait no";

Hash model;
Vehicle vehicle;
Vehicle prevVehicle;
VehicleExtensions ext;
Player player;
Ped playerPed;

int prevNotification;

std::vector<Preset> presets;
std::vector<Preset> saved;

Controls controls;
Settings settings(settingsFile);

float frontCamber;
float frontDistance;
float frontHeight;

float rearCamber;
float rearDistance;
float rearHeight;

bool autoApplied = false;

auto offsetCamber = 0x008;
auto offsetinvCamber = 0x010;
auto offsetDistance = 0x030;
auto offsetHeight = 0x038;

std::string currentInput = "";
bool presethighlighted = false;

void getStats(Vehicle handle) {
	auto numWheels = ext.GetNumWheels(handle);
	if (numWheels < 4)
		return;

	auto wheelPtr = ext.GetWheelsPtr(handle);  // pointer to wheel pointers
	auto wheelAddr0 =	*reinterpret_cast< uint64_t *     >(wheelPtr + 0x008 * 0);
	frontCamber =		*reinterpret_cast< const float *  >(wheelAddr0 + offsetCamber);
	frontDistance =	   -*reinterpret_cast< const float * >(wheelAddr0 + offsetDistance);
	frontHeight =		*reinterpret_cast< const float *  >(wheelAddr0 + offsetHeight);

	auto wheelAddr2 =	*reinterpret_cast< uint64_t *     >(wheelPtr + 0x008 * 2);
	rearCamber =		*reinterpret_cast< const float *  >(wheelAddr2 + offsetCamber);
	rearDistance =	   -*reinterpret_cast< const float * >(wheelAddr2 + offsetDistance);
	rearHeight =		*reinterpret_cast< const float *  >(wheelAddr2 + offsetHeight);

}

void ultraSlam(Vehicle handle, float camberFront, float camberRear, float distanceFront, float distanceRear, float heightFront, float heightRear) {
	auto numWheels = ext.GetNumWheels(handle);
	if (numWheels < 4)
		return;

	auto wheelPtr = ext.GetWheelsPtr(handle);  // pointer to wheel pointers


	for (auto i = 0; i < numWheels; i++) {
		float camber;
		float distance;
		float height;
		if (i == 0 || i ==  1) {
			camber = camberFront;
			distance = distanceFront;
			height = heightFront;
		} else {
			camber = camberRear;
			distance = distanceRear;
			height = heightRear;
		}

		float flip = i % 2 == 0 ? 1.0f : -1.0f; // cuz the wheels on the other side
		auto wheelAddr = *reinterpret_cast<uint64_t *>(wheelPtr + 0x008 * i);
		*reinterpret_cast<float *>(wheelAddr + offsetCamber) = camber * flip;
		*reinterpret_cast<float *>(wheelAddr + offsetinvCamber) = -camber * flip;
		*reinterpret_cast<float *>(wheelAddr + offsetDistance) = -distance * flip;
		*reinterpret_cast<float *>(wheelAddr + offsetHeight) = height;
	}
}

void init() {
	settings.ReadSettings(&controls);
	menu.LoadMenuTheme(menuStyleLocation);

	// Depending on how crappy the XML is this shit might crash and burn.
	try {
		presets = settings.ReadPresets(presetsFile);
		saved = settings.ReadPresets(savedCarsFile);
	}
	catch (...) {
		prevNotification = showNotification("Unknown read error!", prevNotification);
	}
}


void savePreset(bool asPreset, std::string presetName) {
	std::string name = VEHICLE::GET_DISPLAY_NAME_FROM_VEHICLE_MODEL(model);
	std::string plate;
	struct Preset::WheelInfo front = { frontCamber, frontDistance, frontHeight};
	struct Preset::WheelInfo rear = { rearCamber, rearDistance, rearHeight };

	if (asPreset) {
		// Blocking? Don't want other things to happen.
		if (presetName.empty()) {
			plate = Preset::ReservedPlate();
		} else {
			plate = presetName;
		}
	} else {
		plate = VEHICLE::GET_VEHICLE_NUMBER_PLATE_TEXT(vehicle);
	}

	bool alreadyPresent = false;

	for (auto preset : asPreset ? presets : saved) {
		if (plate == preset.Plate() &&
			name == preset.Name()) {
			alreadyPresent = true;
		}
	}

	if (alreadyPresent) {
		settings.OverwritePreset(Preset(front, rear, name, plate), asPreset ? presetsFile : savedCarsFile);
		prevNotification = showNotification(asPreset ? "Updated preset" : "Updated car", prevNotification);
	}
	else {
		settings.AppendPreset(Preset(front, rear, name, plate), asPreset ? presetsFile : savedCarsFile);
		prevNotification = showNotification(asPreset ? "Saved new preset" : "Saved new car", prevNotification);
	}
	init();
}

// ideally this is only done when the thing is highlighted.
std::string evaluateInput() {
	
	PLAYER::IS_PLAYER_CONTROL_ON(false);
	UI::HIDE_HUD_AND_RADAR_THIS_FRAME();
	UI::SET_PAUSE_MENU_ACTIVE(false);
	CONTROLS::DISABLE_ALL_CONTROL_ACTIONS(1);
	CONTROLS::IS_CONTROL_ENABLED(playerPed, false);

	for (char c = ' '; c < '~'; c++) {
		if (IsKeyJustUp(str2key(std::string(1, c)))) {
			currentInput += c;
		}
		if (IsKeyJustUp(str2key("DELETE"))) {
			currentInput.pop_back();
		}
		if (IsKeyJustUp(str2key("BACKSPACE"))) {
			currentInput.clear();
		}
	}
	
	return currentInput;
}
void clearmenustuff() {
	currentInput.clear();

}

// fuck it, hard-code time :D
void deletePreset(Preset preset, std::vector<Preset> fromWhich) {
	std::string fromFile;
	std::string message = "Couldn't find " + preset.Name() + " " + preset.Plate() + " :(";
	if (fromWhich == presets) {
		fromFile = presetsFile;
	}
	if (fromWhich == saved) {
		fromFile = savedCarsFile;
	}
	if (fromFile.empty()) {
		prevNotification = showNotification((char *)message.c_str(), prevNotification);
		return;
	}

	if (settings.DeletePreset(preset, fromFile)) {
		message = "Pang! Deleted " + preset.Name() + " " + preset.Plate();
		init();
	}
	prevNotification = showNotification((char *)message.c_str(), prevNotification);
}

void update_menu() {
	menu.CheckKeys(&controls, std::bind(init), std::bind(clearmenustuff));

	if (menu.CurrentMenu("mainmenu")) {
		menu.Title("Slam It v2"); // TODO: Less sucky names

		menu.MenuOption("Suspension menu", "suspensionmenu");
		menu.MenuOption("Load a preset", "presetmenu");
		menu.MenuOption("List car configs", "carsmenu");
		if (menu.Option("Save as car")) {
			savePreset(false,"");
		}
		std::vector<std::string> derp = { "Enter preset name", currentInput };
		if (menu.OptionPlus("Save as preset", derp , &presethighlighted, nullptr, nullptr)) {
			savePreset(true , derp[1]);
			currentInput.clear();
		}
		if (presethighlighted) {
			evaluateInput();
			presethighlighted = false;
		}
		if (menu.BoolOption("Auto apply cars", &settings.autoApply)) { settings.SaveSettings(); }
		if (menu.BoolOption("Enable mod",		&settings.enableMod)) { settings.SaveSettings(); }

	}

	if (menu.CurrentMenu("suspensionmenu")) {
		menu.Title("Suspension menu");

		menu.FloatOption( "Front Camber",	  &frontCamber,   -2.0f, 2.0f, 0.01f);
		menu.FloatOption( "Front Distance", &frontDistance,  -2.0f, 2.0f,  0.01f);
		menu.FloatOption( "Front Height",   &frontHeight,   -2.0f, 2.0f, 0.01f);
							 											   
		menu.FloatOption( "Rear  Camber",    &rearCamber,    -2.0f, 2.0f, 0.01f); 
		menu.FloatOption( "Rear  Distance",  &rearDistance,   -2.0f, 2.0f,  0.01f);
		menu.FloatOption( "Rear  Height",    &rearHeight,    -2.0f, 2.0f, 0.01f); 
	}

	// Unique name (1 per car model)
	if (menu.CurrentMenu("presetmenu")) {
		menu.Title("Load preset");
		for (auto preset : presets) {
			std::string label = preset.Name() + " " + preset.Plate();
			char * label_ = (char *)label.c_str();
			std::vector<std::string> info;
			info.push_back("Press RIGHT to delete preset");
			info.push_back("Front Camber    " + std::to_string(preset.Front.Camber));
			info.push_back("Front Distance  " + std::to_string(preset.Front.Distance));
			info.push_back("Front Height    " + std::to_string(preset.Front.Height));
			info.push_back("Rear  Camber    " + std::to_string(preset.Rear.Camber));
			info.push_back("Rear  Distance  " + std::to_string(preset.Rear.Distance));
			info.push_back("Rear  Height    " + std::to_string(preset.Rear.Height));
			if (menu.OptionPlus(label_, info, nullptr, std::bind(deletePreset, preset, presets), nullptr)) {
				ultraSlam(vehicle,
						  preset.Front.Camber,
						  preset.Rear.Camber,
						  preset.Front.Distance,
						  preset.Rear.Distance,
						  preset.Front.Height,
						  preset.Rear.Height);
				getStats(vehicle);
				prevNotification = showNotification("Applied preset!", prevNotification);
			}
		}
	}

	// Unique name + plate
	if (menu.CurrentMenu("carsmenu")) {
		menu.Title("Car overview");
		for (auto preset : saved) {
			std::string label = preset.Name() + " " + preset.Plate();
			char * label_ = (char *)label.c_str();
			std::vector<std::string> info;
			info.push_back("Press RIGHT to delete preset");
			info.push_back("Front Camber    " + std::to_string(preset.Front.Camber));
			info.push_back("Front Distance  " + std::to_string(preset.Front.Distance));
			info.push_back("Front Height    " + std::to_string(preset.Front.Height));
			info.push_back("Rear  Camber    " + std::to_string(preset.Rear.Camber));
			info.push_back("Rear  Distance  " + std::to_string(preset.Rear.Distance));
			info.push_back("Rear  Height    " + std::to_string(preset.Rear.Height));
			if (menu.OptionPlus(label_, info, nullptr, std::bind(deletePreset, preset, saved), nullptr)) {
				ultraSlam(vehicle,
						  preset.Front.Camber,
						  preset.Rear.Camber,
						  preset.Front.Distance,
						  preset.Rear.Distance,
						  preset.Front.Height,
						  preset.Rear.Height);
				getStats(vehicle);
				prevNotification = showNotification("Applied preset!", prevNotification);
			}
		}
	}
	menu.EndMenu();
}



void update_game() {
	player = PLAYER::PLAYER_ID();
	playerPed = PLAYER::PLAYER_PED_ID();

	// check if player ped exists and control is on (e.g. not in a cutscene)
	if (!ENTITY::DOES_ENTITY_EXIST(playerPed) || !PLAYER::IS_PLAYER_CONTROL_ON(player))
		return;

	// check for player ped death and player arrest
	if (ENTITY::IS_ENTITY_DEAD(playerPed) || PLAYER::IS_PLAYER_BEING_ARRESTED(player, TRUE))
		return;

	vehicle = PED::GET_VEHICLE_PED_IS_IN(playerPed, false);

	if (!ENTITY::DOES_ENTITY_EXIST(vehicle)) {
		prevVehicle = 0;
		autoApplied = false;
		return;
	}

	model = ENTITY::GET_ENTITY_MODEL(vehicle);
	if (!VEHICLE::IS_THIS_MODEL_A_CAR(model) && !VEHICLE::IS_THIS_MODEL_A_QUADBIKE(model))
		return;

	if (prevVehicle != vehicle) {
		ext.ClearAddress();
		ext.GetAddress(vehicle);
		getStats(vehicle);
		prevVehicle = vehicle;
		autoApplied = false;
		return;
	}

	if (!settings.enableMod)
		return;

	if (settings.autoApply && !autoApplied) {
		for (auto preset : saved) {
			if (VEHICLE::GET_VEHICLE_NUMBER_PLATE_TEXT(vehicle) == preset.Plate() &&
				VEHICLE::GET_DISPLAY_NAME_FROM_VEHICLE_MODEL(model) == preset.Name()) {
				ultraSlam(vehicle, preset.Front.Camber, preset.Rear.Camber, preset.Front.Distance, preset.Rear.Distance, preset.Front.Height, preset.Rear.Height);
				autoApplied = true;
				getStats(vehicle);
				prevNotification = showNotification("Applied preset automatically!", prevNotification);
			}
		}
	}

	ultraSlam(vehicle, frontCamber, rearCamber, frontDistance, rearDistance, frontHeight, rearHeight);
}



void main() {
	init();
	while (true) {
		update_game();
		update_menu();
		WAIT(0);
	}
}

void ScriptMain() {
	srand(GetTickCount());
	main();
}
