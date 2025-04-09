#include "utilsShareMQTT.h"

namespace utils
{
namespace share
{

tReceivedMessages g_ReceivedMessages;

std::vector<tReceivePacketResult> ReceivePacket(tcp::socket& socket, std::vector<std::uint8_t>& buffer)
{
	boost::system::error_code Error;
	const std::size_t SizeRcv = socket.read_some(boost::asio::buffer(buffer), Error);
	std::vector<tReceivePacketResult> ReceivedPackets;
	std::size_t SizeRcvRemains = SizeRcv;
	mqtt::tSpan Span(buffer, SizeRcvRemains);
	while (SizeRcvRemains)
	{
		g_Log.WriteHex(true, utils::log::tColor::LightCyan, "RCV", std::vector<std::uint8_t>(Span.begin(), Span.end()));

		auto Res = mqtt::TestPacket(Span);
		if (!Res.has_value())
			THROW_RUNTIME_ERROR("Received data hasn't been parsed.");
		ReceivedPackets.push_back({ *Res, mqtt::tSpan(buffer.cbegin() + SizeRcv - SizeRcvRemains, SizeRcvRemains - Span.size())});
		SizeRcvRemains = Span.size();
	}
	return ReceivedPackets;
}

void TaskReceiveHandler(tcp::socket& socket)
{
	std::vector<std::uint8_t> ReceiveBuffer(128); //[#] max received packet

	while (true)
	{
		std::vector<tReceivePacketResult> ReceivedPackets = ReceivePacket(socket, ReceiveBuffer); // blocking
		if (ReceivedPackets.empty())
			THROW_RUNTIME_ERROR("No data has been received.");

		for (const auto& [ControlPacketType, PacketSpan] : ReceivedPackets)
		{
			g_Log.WriteHex(true, utils::log::tColor::Cyan, "RCV", std::vector<std::uint8_t>(PacketSpan.begin(), PacketSpan.end()));

			g_ReceivedMessages.Put(ControlPacketType, PacketSpan);

			g_ReceivedMessages.Notify(ControlPacketType);
		}		
	}
}

}
}