#include "main.h"

#include <utilsShare.h>
#include <utilsShareMQTT.h>

void TaskConnectionHandler(std::string_view host, std::string_view service, const std::string& sensorData)
{
	constexpr std::uint16_t KeepAlive = 15; // sec. //[#] - TaskTransactionWait(..) should be taken into consideration

	utils::share::tConnection Connection(host, service, KeepAlive);

	//const bool SessionPresent = 
	Connection.Connect(mqtt::tSessionStateRequest::Continue, "duper_star_SensorA", mqtt::tQoS::AtMostOnceDelivery, true, "SensorA_will", "something wrong has happened"); // 1883

	//if (!SessionContinue)
	Connection.Subscribe({ "SensorA_Settings", mqtt::tQoS::ExactlyOnceDelivery });

	Connection.Publish_AtMostOnceDelivery(true, "SensorA_DateTime_0", std::vector<std::uint8_t>(sensorData.begin(), sensorData.end()));

	Connection.Publish_AtLeastOnceDelivery(true, false, "SensorA_DateTime_1", std::vector<std::uint8_t>(sensorData.begin(), sensorData.end()));

	Connection.Publish_ExactlyOnceDelivery(true, false, "SensorA_DateTime_2", std::vector<std::uint8_t>(sensorData.begin(), sensorData.end()));

	Connection.Disconnect();
}
