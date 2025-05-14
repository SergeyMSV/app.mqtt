///////////////////////////////////////////////////////////////////////////////////////////////////
// utilsShareMQTT
// 2025-04-04
// C++20
///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <libConfig.h>

#include <condition_variable>
#include <deque>
#include <future>
#include <map>
#include <mutex>
#include <optional>
#include <thread>
#include <type_traits>
#include <utility>

#include <boost/asio.hpp>

#include <utilsException.h>
#include <utilsPacketMQTTv3_1_1.h>
#include <utilsShare.h>

using boost::asio::ip::tcp;
namespace mqtt = utils::packet::mqtt_3_1_1;

namespace utils
{
namespace share
{
namespace hidden
{

template<std::size_t QueueCapacity>
class tReceivedMessages
{
	std::map<mqtt::tControlPacketType, std::deque<std::vector<std::uint8_t>>> m_Queue;
	std::mutex m_Mtx;

	std::condition_variable m_CondVar;
	std::mutex m_CondVarMtx;
	mqtt::tControlPacketType m_CondVarControlPacketType = mqtt::tControlPacketType::_None;

public:
	void Put(mqtt::tControlPacketType packType, std::vector<std::uint8_t>& packData)
	{
		std::lock_guard<std::mutex> Guard(m_Mtx);
		m_Queue[packType].push_back(std::move(packData));

		if (m_Queue[packType].size() > QueueCapacity)
			m_Queue[packType].pop_front();
	}
	//void Put(mqtt::tControlPacketType packType, mqtt::tSpan packData)
	//{
	//	std::lock_guard<std::mutex> Guard(m_Mtx);
	//	m_Queue[packType].emplace_back(packData.begin(), packData.end());
	//}
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
	void NotifyBrokenConnection() // the connection has been broken.
	{
		m_CondVar.notify_all();
		m_CondVarControlPacketType = mqtt::tControlPacketType::_None;
	}
};

}

class tConnection
{
	using tPacketData = std::pair<mqtt::tControlPacketType, std::vector<std::uint8_t>>;

	boost::asio::io_context m_ioc;
	std::unique_ptr<tcp::socket> m_Socket;
	std::future<void> m_FutureReceiver;
	std::recursive_mutex m_TransactionMtx;
	utils::chrono::tTimePoint m_TransactionTime;
	std::thread m_KeepConnectionThread;
	bool m_KeepConnection;
	hidden::tReceivedMessages<5> m_ReceivedMessages; // [#]
	const std::uint16_t m_KeepAlive;
	std::uint16_t m_PacketId;

public:
	tConnection() = delete;
	tConnection(std::string_view host, std::string_view service, std::uint16_t keepAlive);
	~tConnection();

	bool Connect(mqtt::tSessionStateRequest sessionStateRequest, const std::string& clientId, mqtt::tQoS willQos, bool willRetain, const std::string& willTopic, const std::string& willMessage);
	bool Connect(mqtt::tSessionStateRequest sessionStateRequest, const std::string& clientId);
	void Publish_AtMostOnceDelivery(bool retain, const std::string& topicName, const std::vector<std::uint8_t>& payload);
	void Publish_AtMostOnceDelivery(bool retain, const std::string& topicName);
	void Publish_AtLeastOnceDelivery(bool retain, bool dup, const std::string& topicName, const std::vector<std::uint8_t>& payload);
	void Publish_ExactlyOnceDelivery(bool retain, bool dup, const std::string& topicName, const std::vector<std::uint8_t>& payload);
	void Subscribe(const mqtt::tSubscribeTopicFilter& topicFilter);
	void Subscribe(const std::vector<mqtt::tSubscribeTopicFilter>& topicFilters);
	void Unsubscribe(const mqtt::tString& topicFilter);
	void Unsubscribe(const std::vector<mqtt::tString>& topicFilters);
	void Ping();
	void Disconnect();

	bool IsConnected() const;

private:
	void KeepConnectionAlive();

	std::vector<tPacketData> ReceivePacket();
	void TaskReceiver();

	bool IsReceiverInOperation() const;

	template<typename T>
	std::optional<typename T::response_type> Transaction(const T& packet)
	{
		std::lock_guard Lock(m_TransactionMtx);
		std::future<std::optional<typename T::response_type>> TaskFuture = std::async(std::launch::async, [&]() { return TaskTransactionHandler<T>(packet); });
		TaskTransactionWait(TaskFuture, 10000, mqtt::ToString(T::GetControlPacketType())); // [#] 10000 is ok for all types of packets ? - it can be = [TBD] keepAlive at most.
		auto Res = TaskFuture.get();
		m_TransactionTime = utils::chrono::tClock::now();
		return Res; 
	}

	template<class T>
	void TaskTransactionWait(std::future<T>& future, std::uint32_t time_ms, const std::string& label)
	{
		const std::uint32_t Pause = 10; // ms
		std::uint32_t PauseCount = time_ms / Pause;
		while (future.wait_for(std::chrono::milliseconds(Pause)) != std::future_status::ready && IsReceiverInOperation())
		{
			if (!PauseCount)
				break;
			--PauseCount;
		}

		if (!IsReceiverInOperation())
			m_ReceivedMessages.NotifyBrokenConnection();
	}

	template <class tCmd>
	std::optional<typename tCmd::response_type> TaskTransactionHandler(const tCmd& packet)
	{
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
	}
};

}
}
