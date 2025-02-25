#include <array>
#include <iostream>
#include <boost/asio.hpp>

#include <utilsPacketMQTT.h>

using boost::asio::ip::tcp;
//using utilsMQTT = utils::packet_MQTT;

int main()
{
	boost::asio::io_context ioc;

	try
	{

		tcp::resolver resolver(ioc);

		tcp::resolver::results_type Ep = resolver.resolve("test.mosquitto.org", "1883"); // MQTT, unencrypted, unauthenticated
		//tcp::resolver::results_type Endpoints = resolver.resolve("test.mosquitto.org", "1884"); // 1884 : MQTT, unencrypted, authenticated

		tcp::socket Socket(ioc);
		boost::asio::connect(Socket, Ep);

		utils::packet_MQTT::tPacketCONNECT Pack("my_client_id", "my_will_topic", "my_will_message"); // 1883

		//utils::packet_MQTT::tPacketCONNECT Pack("my_client_id", "my_will_topic", "my_will_message", "rw", "readwrite"); // 1884; read / write access to the # topic hierarchy
		//utils::packet_MQTT::tPacketCONNECT Pack("my_client_id", "my_will_topic", "my_will_message", "ro", "readonly"); // 1884; read only access to the # topic hierarchy
		//utils::packet_MQTT::tPacketCONNECT Pack("my_client_id", "my_will_topic", "my_will_message", "wo", "writeonly"); // 1884; write only access to the # topic hierarchy

		auto PackVector = Pack.ToVector();
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
				if (*Res == utils::packet_MQTT::tControlPacketType::CONNACK)
				{
					Span = utils::packet_MQTT::tSpan(Buffer, SizeRcv);
					auto Pack_parsed = utils::packet_MQTT::tPacketCONNACK::Parse(Span);
					if (Pack_parsed.has_value())
					{
						std::cout << "CONNACK: " << (int)Pack_parsed->GetVariableHeader().value().ConnectReturnCode << '\n';
					}
				}
				int dfgdfg = 234;
			}


			if (Error == boost::asio::error::eof)
				break; // Connection closed cleanly by peer.
			else if (Error)
				throw boost::system::system_error(Error); // Some other error.

			//std::cout.write(buf.data(), len);
		}
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

	return 0;
}