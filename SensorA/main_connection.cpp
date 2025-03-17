//#include <array>
#include <future>
#include <iostream>
#include <optional>
//#include <thread>
#include <utility>

#include <boost/asio.hpp>

#include <utilsException.h>
#include <utilsExits.h>
#include <utilsPacketMQTT.h>

using boost::asio::ip::tcp;
//using namespace utils::packet;
namespace mqtt = utils::packet::mqtt;
//namespace mqtt = utils::packet_MQTT;

std::vector<std::uint8_t> g_ReceiveBuffer(128);
//std::atomic<bool> g_Exit = false;

using tReceivePacketResult = std::pair<mqtt::tControlPacketType, mqtt::tSpan>;

std::optional<tReceivePacketResult> ReceivePacket(tcp::socket& socket, std::vector<std::uint8_t>& buffer)
{
	boost::system::error_code Error;
	std::size_t SizeRcv = socket.read_some(boost::asio::buffer(buffer), Error);
	if (!SizeRcv)
		return {};

	mqtt::tSpan Span(buffer, SizeRcv);
	auto Res = mqtt::TestPacket(Span);
	if (!Res.has_value())
		THROW_RUNTIME_ERROR("PREVED: NO PACKET 1"); // Res.error() - put it into the message

	return tReceivePacketResult{ *Res, mqtt::tSpan(buffer, SizeRcv) };
}

//std::size_t ReceivePacket(tcp::socket& socket, utils::packet_MQTT::tControlPacketType& packetType)
//{
//	std::vector<std::uint8_t> Buffer(128);
//
//	boost::system::error_code Error;
//
//	//for (;;)
//	//{
//		std::size_t SizeRcv = socket.read_some(boost::asio::buffer(Buffer), Error);
//		if (SizeRcv)
//		{
//			utils::packet_MQTT::tSpan Span(Buffer, SizeRcv);
//			auto Res = utils::packet_MQTT::TestPacket(Span);
//			if (!Res.has_value())
//				THROW_RUNTIME_ERROR("PREVED: NO DATA 1");
//			packetType = *Res;
//			return SizeRcv;
//		}
//	//}
//}

void TaskConnectHandler(tcp::socket& socket)
//utils::packet_MQTT::tPacketCONNACK TaskConnectHandler(tcp::socket& socket)
{
	mqtt::tPacketCONNECT PackCONNECT(false, 10, "duper_star", mqtt::tQoS::AtLeastOnceDelivery, true, "SensorA_will", "something wrong has happened"); // 1883

	std::cout << PackCONNECT.ToString() << '\n';

	auto PackVector = PackCONNECT.ToVector();

	socket.write_some(boost::asio::buffer(PackVector));

	auto Received = ReceivePacket(socket, g_ReceiveBuffer);
	if (!Received.has_value())
		THROW_RUNTIME_ERROR("ER: no connection");

	if (Received->first != mqtt::tControlPacketType::CONNACK)
		THROW_RUNTIME_ERROR("ER: WRONG PACKET 1");

	auto Pack_parsed = mqtt::tPacketCONNACK::Parse(Received->second);
	if (!Pack_parsed.has_value())
		THROW_RUNTIME_ERROR("ER: WRONG PACKET 2"); // Res.error() - put it into the message

	std::cout << Pack_parsed->ToString() << '\n';

	//return static_cast<utils::packet_MQTT::tPacketCONNACK>(Pack_parsed.value());
}
