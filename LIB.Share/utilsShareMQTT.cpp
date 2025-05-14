#include "utilsShareMQTT.h"
#include "utilsStd.h"

#ifndef LIB_UTILS_SHARE_MQTT_CONNECTION_RECEIVE_BUFFER_SIZE
#define LIB_UTILS_SHARE_MQTT_CONNECTION_RECEIVE_BUFFER_SIZE 128
#endif

namespace utils
{
namespace share
{

tConnection::tConnection(std::string_view host, std::string_view service, std::uint16_t keepAlive)
	:m_KeepConnection(false), m_KeepAlive(keepAlive), m_PacketId(0)

{
	tcp::resolver Resolver(m_ioc);
	tcp::resolver::results_type Ep = Resolver.resolve(host, service);
	m_Socket = std::make_unique<tcp::socket>(m_ioc);
	boost::asio::connect(*m_Socket, Ep);
	m_FutureReceiver = std::async(std::launch::async, [&]() { TaskReceiver(); });
	m_KeepConnectionThread = std::thread(&tConnection::KeepConnectionAlive, this);
}

tConnection::~tConnection()
{
	m_Socket->close();
	
	m_KeepConnectionThread.join(); // It seems to be that must be before m_FutureReceiver.get(), because this thread calls m_FutureReceiver.wait_for(..).
	try
	{
		m_FutureReceiver.get(); // The receiving operation shall be finished when the socket is closed.
	}
	catch (std::exception& ex)
	{
		g_Log.Exception(ex.what());
	}
}

bool tConnection::Connect(mqtt::tSessionStateRequest sessionStateRequest, const std::string& clientId, mqtt::tQoS willQos, bool willRetain, const std::string& willTopic, const std::string& willMessage)
{
	mqtt::tPacketCONNECT Pack(sessionStateRequest, m_KeepAlive, clientId, willQos, willRetain, willTopic, willMessage);
	auto PackRsp = Transaction(Pack);
	m_KeepConnection = true; // [TBD] It might be a good idea to check if no error occurred.
	return PackRsp.has_value() && PackRsp->GetVariableHeader().ConnectAcknowledgeFlags.Field.SessionPresent;
}

bool tConnection::Connect(mqtt::tSessionStateRequest sessionStateRequest, const std::string& clientId)
{
	mqtt::tPacketCONNECT Pack(sessionStateRequest, m_KeepAlive, clientId);
	auto PackRsp = Transaction(Pack);
	m_KeepConnection = true; // [TBD] It might be a good idea to check if no error occurred.
	return PackRsp.has_value() && PackRsp->GetVariableHeader().ConnectAcknowledgeFlags.Field.SessionPresent;
}

void tConnection::Publish_AtMostOnceDelivery(bool retain, const std::string& topicName, const std::vector<std::uint8_t>& payload)
{
	Transaction(mqtt::tPacketPUBLISH<mqtt::tQoS::AtMostOnceDelivery>(retain, topicName, payload));
}

void tConnection::Publish_AtMostOnceDelivery(bool retain, const std::string& topicName)
{
	Transaction(mqtt::tPacketPUBLISH<mqtt::tQoS::AtMostOnceDelivery>(retain, topicName));
}

void tConnection::Publish_AtLeastOnceDelivery(bool retain, bool dup, const std::string& topicName, const std::vector<std::uint8_t>& payload)
{
	auto PackRsp = Transaction(mqtt::tPacketPUBLISH<mqtt::tQoS::AtLeastOnceDelivery>(retain, dup, topicName, ++m_PacketId, payload));
	if (PackRsp.has_value())
		g_Log.TestMessage("rsp puback: " + std::to_string((int)PackRsp->GetVariableHeader().PacketId.Value));
}

void tConnection::Publish_ExactlyOnceDelivery(bool retain, bool dup, const std::string& topicName, const std::vector<std::uint8_t>& payload)
{
	std::lock_guard Lock(m_TransactionMtx); // There are two transaction in this function, and they must be be executed in sequence.
	auto PackRsp = Transaction(mqtt::tPacketPUBLISH<mqtt::tQoS::ExactlyOnceDelivery>(retain, dup, topicName, ++m_PacketId, payload));
	if (PackRsp.has_value())
	{
		g_Log.TestMessage("rsp pubrec: " + std::to_string((int)PackRsp->GetVariableHeader().PacketId.Value));

		auto PackRsp2 = Transaction(mqtt::tPacketPUBREL(++m_PacketId));
		if (PackRsp2.has_value())
		{
			g_Log.TestMessage("rsp pubcomp: " + std::to_string((int)PackRsp2->GetVariableHeader().PacketId.Value));
		}
	}
}

void tConnection::Subscribe(const mqtt::tSubscribeTopicFilter& topicFilter)
{
	Transaction(mqtt::tPacketSUBSCRIBE(++m_PacketId, topicFilter));
}

void tConnection::Subscribe(const std::vector<mqtt::tSubscribeTopicFilter>& topicFilters)
{
	Transaction(mqtt::tPacketSUBSCRIBE(++m_PacketId, topicFilters));
}

void tConnection::Unsubscribe(const mqtt::tString& topicFilter)
{
	Transaction(mqtt::tPacketUNSUBSCRIBE(++m_PacketId, topicFilter));
}

void tConnection::Unsubscribe(const std::vector<mqtt::tString>& topicFilters)
{
	Transaction(mqtt::tPacketUNSUBSCRIBE(++m_PacketId, topicFilters));
}

void tConnection::Ping()
{
	Transaction(mqtt::tPacketPINGREQ());
}

void tConnection::Disconnect()
{
	m_KeepConnection = false;
	Transaction(mqtt::tPacketDISCONNECT());
}

bool tConnection::IsConnected() const
{
	return IsReceiverInOperation() && m_KeepConnection;
}

void tConnection::KeepConnectionAlive()
{
	try
	{
		while (IsReceiverInOperation())
		{
			if (m_KeepConnection)
			{
				std::uint32_t Time = utils::chrono::GetDuration<std::chrono::seconds>(m_TransactionTime, utils::chrono::tClock::now());
				if (Time >= m_KeepAlive)
					Ping();
			}
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}
	catch (...)
	{
		// [TBD] NOTIFY ALL THREADS THAT SMTH FAILED
	}
}

std::vector<tConnection::tPacketData> tConnection::ReceivePacket()
{
	auto SocketReadSome = [this](std23::vector<std::uint8_t>& rcvdData)->bool
		{
			std23::vector<std::uint8_t> Buffer(LIB_UTILS_SHARE_MQTT_CONNECTION_RECEIVE_BUFFER_SIZE); //[#] max received data of packets

			boost::system::error_code Error;
			const std::size_t SizeRcv = m_Socket->read_some(boost::asio::buffer(Buffer), Error);
			if (!SizeRcv)
				return false;
			Buffer.resize(SizeRcv);
			if (rcvdData.empty())
			{
				rcvdData = std::move(Buffer);
			}
			else
			{
				rcvdData.append_range(Buffer);
			}
			return true;
		};

	std::vector<tPacketData> ReceivedPackets;
	std23::vector<std::uint8_t> ReceivedData;
	std::size_t ReceivedDataRemainsSize = 0;
	do
	{
		if (!SocketReadSome(ReceivedData))
			return ReceivedPackets;
		ReceivedDataRemainsSize = ReceivedData.size();
		mqtt::tSpan Span(ReceivedData, ReceivedDataRemainsSize);
		while (ReceivedDataRemainsSize)
		{
			auto Res = mqtt::TestPacket(Span);
			if (!Res.has_value())
			{
				ReceivedData = Span.ToVector();
				break;
			}
			ReceivedPackets.push_back({ Res->first, Res->second.ToVector() });
			ReceivedDataRemainsSize = Span.size();
		}
	} while (ReceivedDataRemainsSize);
	return ReceivedPackets;
}

void tConnection::TaskReceiver()
{
	while (true)
	{
		std::vector<tPacketData> ReceivedPackets = ReceivePacket(); // blocking
		if (ReceivedPackets.empty())
		{
			m_ReceivedMessages.NotifyBrokenConnection();
			return;
		}

		for (auto& [ControlPacketType, PacketVector] : ReceivedPackets)
		{
			g_Log.PacketReceivedRaw(PacketVector);

			if (HandlePacket(ControlPacketType, PacketVector))
				continue;

			m_ReceivedMessages.Put(ControlPacketType, PacketVector);

			m_ReceivedMessages.Notify(ControlPacketType);
		}
	}
}

bool tConnection::HandlePacket(mqtt::tControlPacketType packType, std::vector<std::uint8_t>& packData)
{
	mqtt::tSpan PacketRawSpan(packData);
	switch (packType)
	{
	case mqtt::tControlPacketType::PUBLISH:
	{
		auto Pack_parsed = mqtt::tPacketPUBLISH_Parse::Parse(PacketRawSpan);
		if (!Pack_parsed.has_value())
			THROW_RUNTIME_ERROR("Received response has not been parsed."); // Res.error() - put it into the message
		// [TBD] Apply data from the packet
		switch (Pack_parsed->GetFixedHeader().GetQoS())
		{
		case mqtt::tQoS::AtMostOnceDelivery:
			break;
		case mqtt::tQoS::AtLeastOnceDelivery:
			// [TBD] send PUBACK
			break;
		case mqtt::tQoS::ExactlyOnceDelivery:
			// [TBD] send PUBREC
			break;
		}
		return true;
	}
	case mqtt::tControlPacketType::PUBREL:
	{
		auto Pack_parsed = mqtt::tPacketPUBREL::Parse(PacketRawSpan);
		if (!Pack_parsed.has_value())
			THROW_RUNTIME_ERROR("Received response has not been parsed."); // Res.error() - put it into the message

		// [TBD] send PUBCOMP

		return true;
	}
	}
	return false;

	/*
		using tRsp = tCmd::response_type;

		utils::share::tMeasureDuration Measure("TTH");

		auto PackVector = packet.ToVector();

		g_Log.PacketSent(packet.ToString(), PackVector);

		m_ReceivedMessages.Clear(tRsp::GetControlPacketType());

		m_Socket->write_some(boost::asio::buffer(PackVector));

		if (std::is_same_v<tRsp, mqtt::tPacketNOACK>)
			return {};

		m_ReceivedMessages.Wait(tRsp::GetControlPacketType());

		std::vector<std::uint8_t> PacketRaw = m_ReceivedMessages.Get(tRsp::GetControlPacketType());
		if (PacketRaw.empty())
			THROW_RUNTIME_ERROR("No data has been received.");

		mqtt::tSpan PacketRawSpan(PacketRaw);
		auto Pack_parsed = tRsp::Parse(PacketRawSpan);
		if (!Pack_parsed.has_value())
			THROW_RUNTIME_ERROR("Received response has not been parsed."); // Res.error() - put it into the message
		g_Log.PacketReceived(Pack_parsed->ToString());

		return std::optional<tRsp>(*Pack_parsed);//std::move(*Pack_parsed);
	*/
}

bool tConnection::IsReceiverInOperation() const
{
	return m_FutureReceiver.wait_for(std::chrono::milliseconds(1)) != std::future_status::ready;
}

}
}
