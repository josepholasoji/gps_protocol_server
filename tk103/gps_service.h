#pragma once
#include<string>
#include <sstream>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include "../sdk/data_payload_from_device.h"
#include "../sdk/data_downstream.h"
#include "../sdk/sdk.h"


class gps_service
{
private:
   geolocation_svc::LPGPS gps;

public:
	gps_service();
	~gps_service();

	std::string deviceId;

	//Device => Server specific messages
	const char* deviceLogin(data_payload_from_device*  deviceData);
	const char* deviceFeedback(data_payload_from_device*  deviceData);
	const char* deviceFeedbackEnding(data_payload_from_device*  deviceData);
	const char* deviceHandshake(data_payload_from_device*  deviceData);
	void set_gps(geolocation_svc::LPGPS);
	const std::string gps_service::configure_device();
};

