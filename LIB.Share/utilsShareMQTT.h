#pragma once

#include <libConfig.h>

#include <condition_variable>
#include <deque>
#include <future>
#include <map>
#include <mutex>
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

class tReceivedMessages
{
	std::map<mqtt::tControlPacketType, std::deque<std::vector<std::uint8_t>>> m_Queue;
	std::mutex m_Mtx;

	std::condition_variable m_CondVar;
	std::mutex m_CondVarMtx;
	mqtt::tControlPacketType m_CondVarControlPacketType = mqtt::tControlPacketType::_None;

public:
	//void Put(mqtt::tControlPacketType packType, const std::vector<std::uint8_t>& packData)
	//{
	//	std::lock_guard<std::mutex> Guard(m_Mtx);
	//	m_Queue[packType].push_back(packData);
	//}
	void Put(mqtt::tControlPacketType packType, mqtt::tSpan packData)
	{
		std::lock_guard<std::mutex> Guard(m_Mtx);
		m_Queue[packType].emplace_back(packData.begin(), packData.end());
	}
	std::vector<std::uint8_t> Get(mqtt::tControlPacketType packType)
	{
		std::lock_guard<std::mutex> Guard(m_Mtx);
		if (m_Queue[packType].empty())
			return {};
		std::vector<std::uint8_t> PackData = std::move(m_Queue[packType].front()); // [TBD] check if it works.
		m_Queue[packType].pop_front();
		return PackData;
	}
	void Clear(mqtt::tControlPacketType packType)
	{
		std::lock_guard<std::mutex> Guard(m_Mtx);
		if (m_Queue[packType].empty())
			return;
		m_Queue[packType].clear();
	}

	void Wait(mqtt::tControlPacketType packType)
	{
		m_CondVarControlPacketType = packType;
		std::unique_lock<std::mutex> Lock(m_CondVarMtx);
		m_CondVar.wait(Lock);
	}
	void Notify(mqtt::tControlPacketType packType)
	{
		if (m_CondVarControlPacketType != packType)
			return;
		m_CondVar.notify_one();
		m_CondVarControlPacketType = mqtt::tControlPacketType::_None;
	}
};

extern tReceivedMessages g_ReceivedMessages;

using tReceivePacketResult = std::pair<mqtt::tControlPacketType, mqtt::tSpan>;

std::vector<tReceivePacketResult> ReceivePacket(tcp::socket& socket, std::vector<std::uint8_t>& buffer);

void TaskReceiveHandler(tcp::socket& socket);



template <class tCmd>
std::optional<typename tCmd::response_type> TaskTransactionHandler(tcp::socket& socket, const tCmd& packet)
{
	using tRsp = tCmd::response_type;

	utils::share::tMeasureDuration Measure("TTH");

	g_Log.PacketSent(packet.ToString());

	auto PackVector = packet.ToVector();

	g_Log.WriteHex(true, utils::log::tColor::LightMagenta, "SND", PackVector);

	g_ReceivedMessages.Clear(tCmd::response_type::GetControlPacketType());

	socket.write_some(boost::asio::buffer(PackVector));

	if (std::is_same_v<typename tCmd::response_type, mqtt::tPacketNOACK>)
		return {};

	g_ReceivedMessages.Wait(tCmd::response_type::GetControlPacketType());

	std::vector<std::uint8_t> PacketRaw = g_ReceivedMessages.Get(tCmd::response_type::GetControlPacketType());
	if (PacketRaw.empty())
		THROW_RUNTIME_ERROR("No data has been received.");

	mqtt::tSpan PacketRawSpan(PacketRaw);
	auto Pack_parsed = tRsp::Parse(PacketRawSpan);
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
