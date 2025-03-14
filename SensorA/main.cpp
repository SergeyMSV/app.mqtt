//#include <array>
#include <future>
#include <iostream>
#include <thread>

#include <boost/asio.hpp>

#include <utilsException.h>
#include <utilsExits.h>
#include <utilsPacketMQTT.h>

using boost::asio::ip::tcp;

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
	utils::packet_MQTT::tPacketCONNECT PackCONNECT(false, 10, "duper_star", utils::packet_MQTT::tQoS::AtLeastOnceDelivery, true, "SensorA_will", "something wrong has happened"); // 1883

	std::cout << PackCONNECT.ToString() << '\n';

	auto PackVector = PackCONNECT.ToVector();

	socket.write_some(boost::asio::buffer(PackVector));

	std::vector<std::uint8_t> Buffer(128);
	boost::system::error_code Error;
	std::size_t SizeRcv = socket.read_some(boost::asio::buffer(Buffer), Error);
	if (!SizeRcv)
		THROW_RUNTIME_ERROR("PREVED: NO DATA 1");

	utils::packet_MQTT::tSpan Span(Buffer, SizeRcv);
	auto Res = utils::packet_MQTT::TestPacket(Span);
	if (!Res.has_value())
		THROW_RUNTIME_ERROR("PREVED: NO PACKET 1"); // Res.error() - put it into the message

	Span = utils::packet_MQTT::tSpan(Buffer, SizeRcv);

	if (*Res != utils::packet_MQTT::tControlPacketType::CONNACK)
		THROW_RUNTIME_ERROR("PREVED: WRONG PACKET 1");

	auto Pack_parsed = utils::packet_MQTT::tPacketCONNACK::Parse(Span);
	if (!Pack_parsed.has_value())
		THROW_RUNTIME_ERROR("PREVED: WRONG PACKET 2"); // Res.error() - put it into the message

	std::cout << Pack_parsed->ToString() << '\n';

	//return static_cast<utils::packet_MQTT::tPacketCONNACK>(Pack_parsed.value());
}

int main()
{
	int ExitCode = utils::exit_code::EX_OK;

	//try
	//{
		boost::asio::io_context ioc;

		tcp::resolver Resolver(ioc);
		tcp::resolver::results_type Ep = Resolver.resolve("test.mosquitto.org", "1883"); // MQTT, unencrypted, unauthenticated
		//tcp::resolver::results_type Ep = resolver.resolve("test.mosquitto.org", "1884"); // 1884 : MQTT, unencrypted, authenticated

		tcp::socket Socket(ioc);
		boost::asio::connect(Socket, Ep);
	//}
	
	//std::packaged_task<void(tcp::socket&)> TaskConnect(TaskConnectHandler);
	//std::future<void>TaskConnectFuture = TaskConnect.get_future();
	//std::thread TaskConnectThread(std::move(TaskConnect), std::ref(Socket));

	//std::future<void>TaskConnectFuture = std::async(std::launch::async, TaskConnectHandler, std::ref(Socket));

	std::future<void>TaskConnectFuture = std::async(std::launch::deferred, TaskConnectHandler, std::ref(Socket));


	//std::promise<int> ThreadSensorPromise;
	//std::future<int> ThreadSensorFuture = ThreadSensorPromise.get_future();

	//std::thread ThreadSensor(ThreadSensorHandler, std::move(ThreadSensorPromise));

	std::cout << "SOME IMPORTANT WORK STARTED\n";
	Sleep(10000); // some important work...
	std::cout << "SOME IMPORTANT WORK FINISHED\n";

	try
	{
		TaskConnectFuture.get();
		//ExitCode = TaskConnectFuture.get();
	}
	catch (std::exception& ex)
	{
		std::cerr << ex.what() << '\n';

		// g_DataSetMainControl.Thread_GNSS_State = tDataSetMainControl::tStateGNSS::Exit; notify all the threads that it's time to exit

		ExitCode = utils::exit_code::EX_IOERR;
	}

	//TaskConnectThread.join();

	return ExitCode;
}