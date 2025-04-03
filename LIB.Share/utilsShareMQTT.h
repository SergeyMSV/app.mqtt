#pragma once

#include <libConfig.h>

#include <future>
#include <optional>
#include <type_traits>
#include <utility>

#include <boost/asio.hpp>

#include <utilsException.h>
#include <utilsPacketMQTT.h>
#include <utilsShare.h>

using boost::asio::ip::tcp;
namespace mqtt = utils::packet::mqtt;

namespace utils
{
namespace share
{

using tReceivePacketResult = std::pair<mqtt::tControlPacketType, mqtt::tSpan>;

std::optional<tReceivePacketResult> ReceivePacket(tcp::socket& socket, std::vector<std::uint8_t>& buffer);

template <class tCmd>
std::optional<typename tCmd::response_type> TaskTransactionHandler(tcp::socket& socket, const tCmd& packet)
{
	using tRsp = tCmd::response_type;

	utils::share::tMeasureDuration Measure("TTH");

	g_Log.PacketSent(packet.ToString());

	auto PackVector = packet.ToVector();

	socket.write_some(boost::asio::buffer(PackVector));

	if (std::is_same_v<typename tCmd::response_type, mqtt::tPacketNOACK>)
		return {};

	std::vector<std::uint8_t> ReceiveBuffer(128);

	auto ReceivedOpt = ReceivePacket(socket, ReceiveBuffer);
	if (!ReceivedOpt.has_value())
		THROW_RUNTIME_ERROR("No data has been received.");

	auto Pack_parsed = tRsp::Parse(ReceivedOpt->second);
	if (!Pack_parsed.has_value())
		THROW_RUNTIME_ERROR("Received response has not been parsed."); // Res.error() - put it into the message

	g_Log.PacketReceived(Pack_parsed->ToString());

	return std::optional<typename tCmd::response_type>(*Pack_parsed);//std::move(*Pack_parsed);
}

template<class T>
void TaskTransactionWait(std::future<T>& future, std::uint32_t time_ms, const std::string& label)
{
	const std::uint32_t Pause = 10; // ms
	std::uint32_t PauseCount = time_ms / Pause;
	while (future.wait_for(std::chrono::milliseconds(Pause)) != std::future_status::ready)
	{
		if (!PauseCount)
			THROW_RUNTIME_ERROR(label + " exceeded timeout.");

		--PauseCount;
	}
}

}
}
