#include "presets.h"

const std::string Preset::reservedPlate = "_GENERICPRESET_";

Preset::Preset(WheelInfo front, WheelInfo rear, const std::string &name, const std::string &plate) 
	: Front(front)
	, Rear(rear)
	, name(name)
	, plate(plate) {
	// lmao nothing
}

Preset::~Preset() {
}

std::string Preset::ReservedPlate() {
	return reservedPlate;
}

bool Preset::IsPreset() {
	return plate == reservedPlate;
}

std::string Preset::Plate() {
	return plate;
}
std::string Preset::Name() {
	return name;
}