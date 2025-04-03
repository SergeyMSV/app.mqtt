#include "utilsShareMQTT.h"

namespace utils
{
namespace share
{

std::optional<tReceivePacketResult> ReceivePacket(tcp::socket& socket, std::vector<std::uint8_t>& buffer)
{
	boost::system::error_code Error;
	std::size_t SizeRcv = socket.read_some(boost::asio::buffer(buffer), Error);
	if (!SizeRcv)
		return {};

	mqtt::tSpan Span(buffer, SizeRcv);
	auto Res = mqtt::TestPacket(Span);
	if (!Res.has_value())
		THROW_RUNTIME_ERROR("Received data hasn't been parsed.");

	return tReceivePacketResult{ *Res, mqtt::tSpan(buffer, SizeRcv) };
}

}
}