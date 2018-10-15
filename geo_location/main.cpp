#include <zmq.h>
#include <zmq_utils.h>
#include <boost/asio.hpp>
#include <memory>

#include "server.h"
#include "../sdk/sdk.h"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <iostream>
#include <sstream>

#define OTL_ODBC 
#define OTL_ANSI_CPP_11_NULLPTR_SUPPORT
#define OTL_ODBC_SELECT_STM_EXECUTE_BEFORE_DESCRIBE

#include "otlv4.h"

using namespace std;
using boost::property_tree::ptree;


typedef int(__stdcall *f_funci)();
typedef gps*(__stdcall *f_load)(LPGPS_HANDLERS);


//Function signatures
void log_feedback_sql(device_feedback* device_feeback);
void log_feedback(device_feedback* device_feeback);

//global variable declarations
otl_connect db;
const char dir_path[] = "./gps";
LPGPS_HANDLERS handlers = nullptr;
void *zmq_context;
void *zmq_in_socket_handle, *zmq_out_socket_handle;

//system wide functions
void log_feedback(device_feedback* device_feeback) {

	//TODO:load default feedback store from the db, 
	//remove the static assignment below
	__data_store data_store_selection = __data_store::SQLDB;

	if (data_store_selection == __data_store::SQLDB) {
		ptree out;
		out.put("acc_ignition_on", device_feeback->acc_ignition_on);
		out.put("deviceId", device_feeback->deviceId);
		out.put("dlat", device_feeback->dlat);
		out.put("dlon", device_feeback->dlon);
		out.put("dorientation", device_feeback->dorientation);
		out.put("dspeed", device_feeback->dspeed);
		out.put("main_power_switch_on", device_feeback->main_power_switch_on);

		ptree out_date_and_time;
		out.put("_dateTime.day", device_feeback->_dateTime->day);
		out.put("_dateTime.hour", device_feeback->_dateTime->hour);
		out.put("_dateTime.minute", device_feeback->_dateTime->minute);
		out.put("_dateTime.month", device_feeback->_dateTime->month);
		out.put("_dateTime.second", device_feeback->_dateTime->second);
		out.put("_dateTime.year", device_feeback->_dateTime->year);

		out.put("message_type", "GPS_FEEDBACK_MEESAGE_SQL_COMMIT");

		std::ostringstream oss;
		boost::property_tree::write_json(oss, out);
		std::string jsonString = oss.str();

		//send a 0MQ message
		zmq_msg_t msg;
		int rc = zmq_msg_init_size(&msg, jsonString.length());
		assert(rc == 0);
		memcpy(zmq_msg_data(&msg), jsonString.c_str(), jsonString.length());
		rc = zmq_sendmsg(zmq_in_socket_handle, &msg, 0);
		zmq_msg_close(&msg);
		//log_feedback_sql(device_feeback);
	}
	else if (data_store_selection == __data_store::MONGODB) {

	}
	else if (data_store_selection == __data_store::REDIS) {

	}
}

void log_feedback_sql() {

	while (true)
	{
		try
		{
			//connect the sucriber
			void *context = zmq_ctx_new();
			void *subscriber = zmq_socket(context, ZMQ_SUB);
			int rc = zmq_connect(subscriber, "tcp://localhost:5555");
			assert(rc == 0);
			
			//add subriction filter for feeback messages only
			char *filter = "GPS_FEEDBACK_MEESAGE_SQL_COMMIT";
			rc = zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, filter, strlen(filter));
			assert(rc == 0);

			//recieve the 0MQ message
			zmq_msg_t msg;
			rc = zmq_msg_init(&msg);

			assert(rc == 0);
			rc = zmq_recvmsg(subscriber, &msg, 0);

			const char *data = (const char *)malloc(zmq_msg_size(&msg));
			memcpy((void*)data, zmq_msg_data(&msg), zmq_msg_size(&msg));
			assert(rc == 0);
			zmq_msg_close(&msg);

			//
			ptree root(data);
			device_feedback *device_feeback = (device_feedback *)malloc(sizeof(device_feedback));
			device_feeback->acc_ignition_on = root.get<double>("acc_ignition_on", 0);
			strcpy_s(device_feeback->deviceId, sizeof(device_feeback->deviceId) , root.get<std::string>("deviceId").c_str());
			device_feeback->dlat = root.get<double>("dlat", 0);
			device_feeback->dlon = root.get<double>("dlon", 0);
			device_feeback->dmile_data = root.get<double>("dmile_data", 0);
			device_feeback->dorientation = root.get<double>("dorientation", 0);
			device_feeback->dspeed = root.get<double>("dspeed", 0);
			device_feeback->main_power_switch_on = root.get<int>("main_power_switch_on", 0);

			datetime* _datetime = (datetime *)malloc(sizeof(datetime));
			_datetime->hour = root.get<int>("_datetime.hour", 0);
			_datetime->day = root.get<int>("_datetime.day", 0);
			_datetime->minute = root.get<int>("_datetime.minute", 0);
			_datetime->month = root.get<int>("_datetime.month", 0);
			_datetime->second = root.get<int>("_datetime.second", 0);
			_datetime->year= root.get<int>("_datetime.year", 0);
			device_feeback->_dateTime = _datetime;

			//save the location details
			otl_datetime _dateTime;
			_dateTime.day = device_feeback->_dateTime->day;
			_dateTime.month = device_feeback->_dateTime->month;
			_dateTime.year = std::atoi(std::string((device_feeback->_dateTime->year < 10 ? "200" : "20") + std::to_string(device_feeback->_dateTime->year)).c_str());
			_dateTime.hour = device_feeback->_dateTime->hour;
			_dateTime.minute = device_feeback->_dateTime->minute;
			_dateTime.second = device_feeback->_dateTime->second;

			//
			otl_stream o(1,
				"{call add_device_location_log(:time<timestamp,in>,:latitude<double,in>,:longitude<double,in>,:device_id<char[20],in>,:orientation<double,in>,:speed<double,in>,:power_switch_is_on<int,in>,:igintion_is_on<int,in>,:miles_data<double,in>)}",
				db);

			o.set_commit(0);

			o << _dateTime
				<< device_feeback->dlat
				<< device_feeback->dlon
				<< device_feeback->deviceId
				<< device_feeback->dorientation
				<< device_feeback->dspeed
				<< (device_feeback->main_power_switch_on ? 1 : 0)
				<< (device_feeback->acc_ignition_on ? 1 : 0)
				<< (double)device_feeback->dmile_data;
		}
		catch (otl_exception& p) {
			cerr << p.msg << endl; // print out error message
			cerr << p.code << endl; // print out error code
			cerr << p.var_info << endl; // print out the variable that caused the error
			cerr << p.sqlstate << endl; // print out SQLSTATE message
			cerr << p.stm_text << endl; // print out SQL that caused the error
		}
	}
}

bool is_device_registered(const char* deviceId) {
	try {
		int is_device_registered = 0;
		otl_stream o(1, "{call find_device(:device_id<char[20],in>, @registered)}", db);
		o.set_commit(0);
		o << deviceId;

		//read all the outut...
		otl_stream s(1, "select @registered  :#1<int>", db, otl_implicit_select);
		s >> is_device_registered;
		return is_device_registered > 0;
	}
	catch (otl_exception& p) {
		cerr << p.msg << endl; // print out error message
		cerr << p.code << endl; // print out error code
		cerr << p.var_info << endl; // print out the variable that caused the error
		cerr << p.sqlstate << endl; // print out SQLSTATE message
		cerr << p.stm_text << endl; // print out SQL that caused the error
	}

	return false;
}

int main()
{

	try
	{
		//start connection to sql db
		otl_connect::otl_initialize();
		std::string strConn = "DRIVER={MySQL ODBC 8.0 ANSI Driver};SERVER=127.0.0.1;PORT=3306;DATABASE=geolocation_service;USER=root;PASSWORD=;";
		db.rlogon(strConn.c_str());

		//Zero MQ version
		int major, minor, patch;
		zmq_version(&major, &minor, &patch);
		printf("Current 0MQ version is %d.%d.%d\n", major, minor, patch);

		//Start zero mq
		void *zmq_context = zmq_ctx_new();
		zmq_in_socket_handle = zmq_socket(zmq_context, ZMQ_PUB);
		zmq_bind(zmq_in_socket_handle, "tcp://*:5555");
	}
	catch (otl_exception& p)
	{
		cerr << p.msg << endl; // print out error message
		cerr << p.code << endl; // print out error code
		cerr << p.var_info << endl; // print out the variable that caused the error
		cerr << p.sqlstate << endl; // print out SQLSTATE message
		cerr << p.stm_text << endl; // print out SQL that caused the error
	}
	catch (exception ex) {
		cerr << ex.what() << endl; // print out SQL that caused the error
	}

	handlers = (LPGPS_HANDLERS) malloc(sizeof(GPS_HANDLERS));
	if (handlers != nullptr) {
		handlers->log_feedback = log_feedback;
		handlers->is_device_registered = is_device_registered;
	}



	//Search the plugins directory for service plugins
	WIN32_FIND_DATA file = { 0 };
	char path[MAX_PATH] = { 0 };
	GetCurrentDirectory(MAX_PATH, (LPWSTR)path);

	//
	auto  gpses = std::make_shared<std::vector<gps*>>();

	HANDLE search_handle = FindFirstFile(L"services\\*.gps", &file);
	if (search_handle){
		do{
			//load each dynamically...
			HINSTANCE hGetProcIDDLL = LoadLibrary((LPCWSTR)std::wstring(L"services\\").append(file.cFileName).c_str());

			if (!hGetProcIDDLL) {
				std::cout << "could not load the GPS service file" << std::endl;
				return EXIT_FAILURE;
			}

			// resolve function address here
			f_load load = (f_load)GetProcAddress(hGetProcIDDLL, "load");
			if (!load) {
				std::cout << "could not locate the function" << std::endl;
				return EXIT_FAILURE;
			}

			gpses->push_back(load(handlers));
		} while (FindNextFile(search_handle, &file));
		FindClose(search_handle);
	}
			
	boost::asio::io_service io_service;
	std::vector<server> servers;

	//Start the servers...
	for (gps* _gps : *gpses)
	{
		new server(io_service, _gps->serverPort(), _gps);
	}

	io_service.run();

	//prepare of exit
	auto on_exit = []{
		db.logoff();
		free((void*)handlers);
	};

	atexit(on_exit);	
	return 0;
}
