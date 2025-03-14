//#include <array>
#include <future>
#include <iostream>
#include <thread>

#include <boost/asio.hpp>

#include <utilsException.h>
#include <utilsExits.h>
#include <utilsPacketMQTT.h>

using boost::asio::ip::tcp;
//using utilsMQTT = utils::packet_MQTT;

int ThreadSensorHandler(tcp::socket& socket)
//void ThreadSensorHandler(std::promise<int> promise)
{
	//try
	//{
		THROW_RUNTIME_ERROR("smth wrong");
	//}
	//catch (...)
	//{
	//	promise.set_exception(std::current_exception());
	//}
	return 21;
	////////////////////////////////

/*	boost::asio::io_context ioc;

	try
	{
		tcp::resolver Resolver(ioc);
		tcp::resolver::results_type Ep = Resolver.resolve("test.mosquitto.org", "1883"); // MQTT, unencrypted, unauthenticated
		//tcp::resolver::results_type Ep = resolver.resolve("test.mosquitto.org", "1884"); // 1884 : MQTT, unencrypted, authenticated

		tcp::socket Socket(ioc);
		boost::asio::connect(Socket, Ep);

		utils::packet_MQTT::tPacketCONNECT PackCONNECT(false, 10, "duper_star", utils::packet_MQTT::tQoS::AtLeastOnceDelivery, true, "SensorA_will", "something wrong has happened"); // 1883
		
		//utils::packet_MQTT::tPacketCONNECT PackCONNECT("my_client_id", utils::packet_MQTT::tQoS::AtLeastOnceDelivery, true,"my_will_topic", "my_will_message", "rw", "readwrite"); // 1884; read / write access to the # topic hierarchy
		//utils::packet_MQTT::tPacketCONNECT PackCONNECT("my_client_id", utils::packet_MQTT::tQoS::AtLeastOnceDelivery, true,"my_will_topic", "my_will_message", "ro", "readonly"); // 1884; read only access to the # topic hierarchy
		//utils::packet_MQTT::tPacketCONNECT PackCONNECT("my_client_id", utils::packet_MQTT::tQoS::AtLeastOnceDelivery, true,"my_will_topic", "my_will_message", "wo", "writeonly"); // 1884; write only access to the # topic hierarchy

		std::cout << PackCONNECT.ToString() << '\n';

		auto PackVector = PackCONNECT.ToVector();
		Socket.write_some(boost::asio::buffer(PackVector));
		//Socket.write_some(boost::asio::buffer(PackVector.data(), PackVector.size()));

		std::vector<std::uint8_t> Buffer(128);

		boost::system::error_code Error;

		for (;;)
		{
			std::size_t SizeRcv = Socket.read_some(boost::asio::buffer(Buffer), Error);

			if (SizeRcv)
			{
				utils::packet_MQTT::tSpan Span(Buffer, SizeRcv);
				auto Res = utils::packet_MQTT::TestPacket(Span);
				if (!Res.has_value())
					break;

				Span = utils::packet_MQTT::tSpan(Buffer, SizeRcv);

				switch (*Res)
				{
				case utils::packet_MQTT::tControlPacketType::CONNACK:
				{
					auto Pack_parsed = utils::packet_MQTT::tPacketCONNACK::Parse(Span);
					if (Pack_parsed.has_value())
					{
						std::cout << Pack_parsed->ToString() << '\n';

						//utils::packet_MQTT::tPacketPUBLISH PackPUBLISH(false, false, "SensorA_Topic_1");
						utils::packet_MQTT::tPacketPUBLISH PackPUBLISH(false, false, "SensorA_Topic_1", utils::packet_MQTT::tQoS::AtLeastOnceDelivery, 0x1234);
						std::cout << PackPUBLISH.ToString() << '\n';
						auto PackVector = PackPUBLISH.ToVector();
						Socket.write_some(boost::asio::buffer(PackVector));

						Sleep(2000);
						continue;
					}
					break;
				}
				case utils::packet_MQTT::tControlPacketType::PUBACK:
				{
					auto Pack_parsed = utils::packet_MQTT::tPacketPUBACK::Parse(Span);
					if (Pack_parsed.has_value())
					{
						std::cout << Pack_parsed->ToString() << '\n';
					}
					break;
				}
				case utils::packet_MQTT::tControlPacketType::PINGRESP:
				{
					auto Pack_parsed = utils::packet_MQTT::tPacketPINGRESP::Parse(Span);
					if (Pack_parsed.has_value())
					{
						static std::uint32_t Counter = 0;
						std::cout << Pack_parsed->ToString() <<" - counter: " << ++Counter << '\n';
					}
					break;
				}
				default:
				{
					std::cout << "XZ\n";
					break;
				}
				}
			}




			Sleep(PackCONNECT.GetVariableHeader()->KeepAlive.Value * 1000);

			static std::uint32_t CounterDISCONNECT = 0;
			if (++CounterDISCONNECT == 10)
			{
				auto Pack = utils::packet_MQTT::tPacketDISCONNECT();
				std::cout << Pack.ToString() << '\n';
				Socket.write_some(boost::asio::buffer(Pack.ToVector()));
			}
			else
			{
				auto Pack = utils::packet_MQTT::tPacketPINGREQ();
				std::cout << Pack.ToString() << '\n';
				Socket.write_some(boost::asio::buffer(Pack.ToVector()));
			}


			if (Error == boost::asio::error::eof)
				break; // Connection closed cleanly by peer.
			else if (Error)
				throw boost::system::system_error(Error); // Some other error.

			//std::cout.write(buf.data(), len);
		}
	}
	catch (...)
	{
		promise.set_exception(std::current_exception());
		//std::cerr << e.what() << std::endl;
	}*/

	//return 0;
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
	
	std::packaged_task<int(tcp::socket&)> Task1(ThreadSensorHandler);
	std::future<int> ThreadSensorFuture = Task1.get_future();

	std::thread ThreadSensor(std::move(Task1), std::ref(Socket));

	//std::promise<int> ThreadSensorPromise;
	//std::future<int> ThreadSensorFuture = ThreadSensorPromise.get_future();

	//std::thread ThreadSensor(ThreadSensorHandler, std::move(ThreadSensorPromise));

	Sleep(10000);
	std::cout << "PREVED MEDVED\n";

	try
	{
		ExitCode = ThreadSensorFuture.get();
	}
	catch (std::exception& ex)
	{
		std::cerr << ex.what() << '\n';

		// g_DataSetMainControl.Thread_GNSS_State = tDataSetMainControl::tStateGNSS::Exit; notify all the threads that it's time to exit

		ExitCode = utils::exit_code::EX_IOERR;
	}

	ThreadSensor.join();

	return ExitCode;
}