#include "utilsPacketMQTT.h"

#include <algorithm>

namespace utils
{
namespace packet
{
namespace mqtt
{

template<typename T>
typename std::enable_if<std::is_trivially_copyable<T>::value, std::string>::type ToString(T code, const std::string& msg)
{
	return std::string("[") + std::to_string(static_cast<int>(code)) + "] " + msg;
}

const std::vector<std::pair<tControlPacketType, std::string>> LogControlPacketType =
{
	{tControlPacketType::CONNECT, "CONNECT"},
	{tControlPacketType::CONNACK, "CONNACK"},
	{tControlPacketType::PUBLISH, "PUBLISH"},
	{tControlPacketType::PUBACK, "PUBACK"},
	{tControlPacketType::PUBREC, "PUBREC"},
	{tControlPacketType::PUBREL, "PUBREL"},
	{tControlPacketType::PUBCOMP, "PUBCOMP"},
	{tControlPacketType::SUBSCRIBE, "SUBSCRIBE"},
	{tControlPacketType::SUBACK, "SUBACK"},
	{tControlPacketType::UNSUBSCRIBE, "UNSUBSCRIBE"},
	{tControlPacketType::UNSUBACK, "UNSUBACK"},
	{tControlPacketType::PINGREQ, "PINGREQ"},
	{tControlPacketType::PINGRESP, "PINGRESP"},
	{tControlPacketType::DISCONNECT, "DISCONNECT"},
};

const std::vector<std::pair<tQoS, std::string>> LogQoS =
{
	{tQoS::AtMostOnceDelivery, "At most once delivery"},
	{tQoS::AtLeastOnceDelivery, "At least once delivery"},
	{tQoS::ExactlyOnceDelivery, "Exactly once delivery"},
};

const std::vector<std::pair<tConnectReturnCode, std::string>> LogConnectReturnCode =
{
	{tConnectReturnCode::ConnectionAccepted, "Connection Accepted"},
	{tConnectReturnCode::ConnectionRefused_UnacceptableProtocolVersion, "Connection Refused, unacceptable protocol version"},
	{tConnectReturnCode::ConnectionRefused_IdentifierRejected, "Connection Refused, identifier rejected"},
	{tConnectReturnCode::ConnectionRefused_ServerUnavailable, "Connection Refused, Server unavailable"},
	{tConnectReturnCode::ConnectionRefused_BadUserNameOrPassword, "Connection Refused, bad user name or password"},
	{tConnectReturnCode::ConnectionRefused_NotAuthorized, "Connection Refused, not authorized"},
};

const std::vector<std::pair<tSubscribeReturnCode, std::string>> LogSubscribeReturnCode =
{
	{tSubscribeReturnCode::SuccessMaximumQoS_AtMostOnceDelivery, "SuccessMaximumQoS=AtMostOnceDelivery"},
	{tSubscribeReturnCode::SuccessMaximumQoS_AtLeastOnceDelivery, "SuccessMaximumQoS=AtLeastOnceDelivery"},
	{tSubscribeReturnCode::SuccessMaximumQoS_ExactlyOnceDelivery, "SuccessMaximumQoS=ExactlyOnceDelivery"},
	{tSubscribeReturnCode::Failure, "Failure"},
};

template<typename T>
std::string GetString(const std::vector<std::pair<T, std::string>>& list, T id)
{
	auto it = std::ranges::find_if(list, [&id](const std::pair<T, std::string>& i) { return i.first == id; });
	if (it == list.end())
		return ToString(id, "ERROR");
	return ToString(id, it->second);
}

template<>
std::string GetString(const std::vector<std::pair<tControlPacketType, std::string>>& list, tControlPacketType id)
{
	auto it = std::ranges::find_if(list, [&id](const std::pair<tControlPacketType, std::string>& i) { return i.first == id; });
	if (it == list.end())
		return "ERROR";
	return it->second;
}

template<typename T>
std::string ToString(T val)
{
	if constexpr (std::is_same_v<T, tQoS>)
		return GetString(LogQoS, val);

	if constexpr (std::is_same_v<T, tConnectReturnCode>)
		return GetString(LogConnectReturnCode, val);

	if constexpr (std::is_same_v<T, tSubscribeReturnCode>)
		return GetString(LogSubscribeReturnCode, val);

	if constexpr (std::is_same_v<T, bool>)
		return val ? "true" : "false";

	return "ERROR";
}

template<>
std::string ToString(tControlPacketType val)
{
	return GetString(LogControlPacketType, val);
}

std::string ToString(const std::string& label, const std::optional<std::string>& val)
{
	if (val.has_value())
		return label + *val;
	return {};
}

std::string ToString(const std::string& label, const std::optional<tUInt16>& val)
{
	if (val.has_value())
		return label + std::to_string(val->Value);
	return {};
}

std::vector<std::uint8_t> ToVector(const std::optional<tString>& val)
{
	if (val.has_value())
		return val->ToVector();
	return {};
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::optional<tUInt16> tUInt16::Parse(tSpan& data)
{
	if (data.size() < 2)
		return {};
	tUInt16 Val;
	Val.Field.MSB = data[0];
	Val.Field.LSB = data[1];
	data.Skip(2);
	return Val;
}

std::vector<std::uint8_t> tUInt16::ToVector() const
{
	std::vector<std::uint8_t> Data;
	Data.push_back(Field.MSB);
	Data.push_back(Field.LSB);
	return Data;
}

tUInt16& tUInt16::operator=(std::uint16_t val)
{
	Value = val;
	return *this;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::optional<tString> tString::Parse(tSpan& data)
{
	std::optional<tUInt16> LengthOpt = tUInt16::Parse(data);
	if (!LengthOpt.has_value())
		return {};

	if (data.size() < LengthOpt->Value)
		return {};

	std::string Str(data.begin(), data.begin() + LengthOpt->Value);
	data.Skip(LengthOpt->Value);

	return Str;
}

std::vector<std::uint8_t> tString::ToVector() const
{
	std23::vector<std::uint8_t> Data;
	tUInt16 StrSize = static_cast<std::uint8_t>(size());
	Data.append_range(StrSize.ToVector());
	Data.insert(Data.end(), begin(), end());
	return Data;
}

namespace hidden
{

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::optional<std::pair<tFixedHeaderBase, std::size_t>> tFixedHeaderBase::Parse(tSpan& data)
{
	if (data.empty())
		return {};
	tFixedHeaderBase FixedHeader = data[0];
	const auto ControlPacketType = FixedHeader.GetControlPacketType();
	if (ControlPacketType < tControlPacketType::CONNECT || ControlPacketType > tControlPacketType::DISCONNECT)
		return {};
	data.Skip(1);
	auto RLengtOpt = tRemainingLength::Parse(data);
	if (!RLengtOpt.has_value() || *RLengtOpt > data.size())
		return {};
	return std::pair(FixedHeader, *RLengtOpt);
}

std::string tFixedHeaderBase::ToString(bool align) const
{
	std::string Str = mqtt::ToString(GetControlPacketType());
	constexpr std::size_t LenghAligned = 11; // That is size of the longest packet name: "UNSUBSCRIBE".
	if (align && Str.size() < LenghAligned)
		Str.append(LenghAligned - Str.size(), ' ');
	return Str;
}

std::vector<std::uint8_t> tFixedHeaderBase::ToVector(std::size_t dataSize) const
{
	std23::vector<std::uint8_t> Vect = tRemainingLength::ToVector(static_cast<std::uint32_t>(dataSize));
	if (Vect.empty())
		return {};
	Vect.insert(Vect.begin(), Data.Value);
	return Vect;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::optional<std::uint32_t> tRemainingLength::Parse(tSpan& data)
{
	if (data.empty())
		return {};

	auto DataBegin = data.begin();

	std::uint32_t Length = 0;
	tLengthPart Part{};
	for (std::size_t i = 0; DataBegin != data.end() && i < m_SizeMax; ++i)
	{
		Part.Value = *DataBegin++;

		Length |= (std::size_t)Part.Field.Num << (i * 7);

		if (!Part.Field.Continuation)
		{
			data.Skip(i + 1);
			return Length;
		}
	}

	return {};
}

std::vector<std::uint8_t> tRemainingLength::ToVector(std::uint32_t val)
{
	std::vector<std::uint8_t> Data;
	for (int i = 0; i < m_SizeMax; ++i)
	{
		tLengthPart Part{};
		Part.Field.Num = val;

		val = val >> 7;
		if (val)
			Part.Field.Continuation = 1;

		Data.push_back(Part.Value);

		if (!val)
			break;
	}

	if (val)
		return {};

	return Data;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CONNECT

tContentCONNECT::tVariableHeader::tVariableHeader(tVariableHeader&& val) noexcept
	:ProtocolName(std::move(val.ProtocolName)), ProtocolLevel(val.ProtocolLevel), ConnectFlags(val.ConnectFlags), KeepAlive(val.KeepAlive)
{
}

tContentCONNECT::tVariableHeader& tContentCONNECT::tVariableHeader::operator=(tVariableHeader&& val) noexcept
{
	if (this == &val)
		return *this;
	ProtocolName = std::move(val.ProtocolName);
	ProtocolLevel = val.ProtocolLevel;
	ConnectFlags = val.ConnectFlags;
	KeepAlive = val.KeepAlive;
	return *this;
}

bool tContentCONNECT::tVariableHeader::operator==(const tVariableHeader& val) const
{
	return ProtocolName == val.ProtocolName && ProtocolLevel == val.ProtocolLevel && ConnectFlags.Value == val.ConnectFlags.Value && KeepAlive.Value == val.KeepAlive.Value;
}

tContentCONNECT::tPayload::tPayload(tPayload&& val) noexcept
	:ClientId(val.ClientId), WillTopic(std::move(val.WillTopic)), WillMessage(std::move(val.WillMessage)), UserName(std::move(val.UserName)), Password(std::move(val.Password))
{
}

tContentCONNECT::tPayload& tContentCONNECT::tPayload::operator=(tPayload&& val) noexcept
{
	if (this == &val)
		return *this;
	ClientId = val.ClientId;
	WillTopic = std::move(val.WillTopic);
	WillMessage = std::move(val.WillMessage);
	UserName = std::move(val.UserName);
	Password = std::move(val.Password);
	return *this;
}

bool tContentCONNECT::tPayload::operator==(const tPayload& val) const
{
	return ClientId == val.ClientId && WillTopic == val.WillTopic && WillMessage == val.WillMessage && UserName == val.UserName && Password == val.Password;
}

tContentCONNECT::tContentCONNECT(bool cleanSession, std::uint16_t keepAlive, const std::string& clientId, tQoS willQos, bool willRetain, const std::string& willTopic, const std::string& willMessage, const std::string& userName, const std::string& password)
{
	VariableHeader.ProtocolName = DefaultProtocolName;
	VariableHeader.ProtocolLevel = DefaultProtocolLevel;
	VariableHeader.ConnectFlags.Field.CleanSession = cleanSession ? 1 : 0;
	VariableHeader.KeepAlive.Value = keepAlive;

	SetClientId(clientId);
	SetWill(willQos, willRetain, willTopic, willMessage);
	SetUser(userName, password);
}

tContentCONNECT::tContentCONNECT(bool cleanSession, std::uint16_t keepAlive, const std::string& clientId, tQoS willQos, bool willRetain, const std::string& willTopic, const std::string& willMessage)
	:tContentCONNECT(cleanSession, keepAlive, clientId, willQos, willRetain, willTopic, willMessage, "", "")
{
}

tContentCONNECT::tContentCONNECT(bool cleanSession, std::uint16_t keepAlive, const std::string& clientId)
	:tContentCONNECT(cleanSession, keepAlive, clientId, tQoS::AtMostOnceDelivery, false, "", "", "", "")
{
}

tContentCONNECT::tContentCONNECT(bool cleanSession, std::uint16_t keepAlive, const std::string& clientId, const std::string& userName, const std::string& password)
	:tContentCONNECT(cleanSession, keepAlive, clientId, tQoS::AtMostOnceDelivery, false, "", "", userName, password)
{
}

tContentCONNECT::tContentCONNECT(tContentCONNECT&& val) noexcept
	:FixedHeader(val.FixedHeader), VariableHeader(std::move(val.VariableHeader)), Payload(std::move(val.Payload))
{
}

std::optional<tContentCONNECT> tContentCONNECT::Parse(tSpan& data)
{
	std::optional<std::pair<tFixedHeader, std::size_t>> FixedHeaderOpt = tFixedHeader::Parse(data);
	if (!FixedHeaderOpt.has_value())
		return {};

	tContentCONNECT Content{};
	Content.FixedHeader = FixedHeaderOpt->first;

	auto ProtocolNameOpt = tString::Parse(data);
	if (!ProtocolNameOpt.has_value())
		return {};

	Content.VariableHeader.ProtocolName = *ProtocolNameOpt;

	if (data.size() < 2)
		return {};
	Content.VariableHeader.ProtocolLevel = data[0];
	Content.VariableHeader.ConnectFlags.Value = data[1];
	data.Skip(2);

	auto KeepAliveOpt = tUInt16::Parse(data);
	if (!KeepAliveOpt.has_value())
		return {};
	Content.VariableHeader.KeepAlive = *KeepAliveOpt;

	// These fields, if present, MUST appear in the order Client Identifier, Will Topic, Will Message, User Name, Password
	auto StrOpt = tString::Parse(data);
	if (!StrOpt.has_value())
		return {};
	Content.Payload.ClientId = *StrOpt;

	if (Content.VariableHeader.ConnectFlags.Field.WillFlag)
	{
		StrOpt = tString::Parse(data);
		if (!StrOpt.has_value())
			return {};
		Content.Payload.WillTopic = *StrOpt;

		StrOpt = tString::Parse(data);
		if (!StrOpt.has_value())
			return {};
		Content.Payload.WillMessage = *StrOpt;
	}

	if (Content.VariableHeader.ConnectFlags.Field.UserNameFlag)
	{
		StrOpt = tString::Parse(data);
		if (!StrOpt.has_value())
			return {};
		Content.Payload.UserName = StrOpt;

		StrOpt = tString::Parse(data);
		if (!StrOpt.has_value())
			return {};
		Content.Payload.Password = StrOpt;
	}

	return Content;
}

void tContentCONNECT::SetClientId(std::string val)
{
	// 579 The Server MUST allow ClientIds which are between 1 and 23 UTF-8 encoded bytes in length, and that
	// 580 contain only the characters
	// 581 "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ" [MQTT-3.1.3-5].
	constexpr std::size_t ClientIdSizeMax = 23;
	if (val.size() > ClientIdSizeMax)
		val.resize(ClientIdSizeMax);
	// 583 The Server MAY allow ClientId�s that contain more than 23 encoded bytes. The Server MAY allow
	// 584 ClientId�s that contain characters not included in the list given above.
	// 
	// 586 A Server MAY allow a Client to supply a ClientId that has a length of zero bytes, however if it does so the
	// 587 Server MUST treat this as a special case and assign a unique ClientId to that Client. It MUST then
	// 588 process the CONNECT packet as if the Client had provided that unique ClientId [MQTT-3.1.3-6].
	// 
	// 590 If the Client supplies a zero-byte ClientId, the Client MUST also set CleanSession to 1 [MQTT-3.1.3-7].
	Payload.ClientId = val;
}

void tContentCONNECT::SetWill(tQoS qos, bool retain, const std::string& topic, const std::string& message)
{
	VariableHeader.ConnectFlags.Field.WillFlag = !topic.empty() && !message.empty();

	if (VariableHeader.ConnectFlags.Field.WillFlag)
	{
		VariableHeader.ConnectFlags.Field.WillQoS = static_cast<std::uint8_t>(qos);
		VariableHeader.ConnectFlags.Field.WillRetain = retain ? 1 : 0;
		Payload.WillTopic = topic;
		Payload.WillMessage = message;
	}
	else
	{
		VariableHeader.ConnectFlags.Field.WillQoS = 0; // 510 If the Will Flag is set to 0, then the Will QoS MUST be set to 0 (0x00) [MQTT-3.1.2-13].
		VariableHeader.ConnectFlags.Field.WillRetain = 0; // 518 If the Will Flag is set to 0, then the Will Retain Flag MUST be set to 0[MQTT - 3.1.2 - 15].
		Payload.WillTopic.reset();
		Payload.WillMessage.reset();
	}
}

void tContentCONNECT::SetUser(const std::string& name, const std::string& password)
{
	// 532 If the Password Flag is set to 0, a password MUST NOT be present in the payload [MQTT-3.1.2-20].
	// 532 If the Password Flag is set to 1, a password MUST be present in the payload [MQTT-3.1.2-21].
	// 533 If the User Name Flag is set to 0, the Password Flag MUST be set to 0 [MQTT-3.1.2-22].
	VariableHeader.ConnectFlags.Field.UserNameFlag = !name.empty();
	VariableHeader.ConnectFlags.Field.PasswordFlag = !name.empty() && !password.empty();

	if (VariableHeader.ConnectFlags.Field.UserNameFlag)
	{
		Payload.UserName = name;
	}
	else
	{
		Payload.UserName.reset();
	}

	if (VariableHeader.ConnectFlags.Field.PasswordFlag)
	{
		Payload.Password = password;
	}
	else
	{
		Payload.Password.reset();
	}
}

std::string tContentCONNECT::ToString() const
{
	std::string Str = FixedHeader.ToString(true);
	Str += " Protocol";
	Str += " name: " + VariableHeader.ProtocolName;
	Str += ", level: " + std::to_string(VariableHeader.ProtocolLevel);
	Str += "; Clean session: " + mqtt::ToString(static_cast<bool>(VariableHeader.ConnectFlags.Field.CleanSession));
	Str += "; Will";
	Str += " flag: " + mqtt::ToString(static_cast<bool>(VariableHeader.ConnectFlags.Field.WillFlag));
	Str += ", QoS: " + mqtt::ToString(static_cast<tQoS>(VariableHeader.ConnectFlags.Field.WillQoS));
	Str += ", retain: " + mqtt::ToString(static_cast<bool>(VariableHeader.ConnectFlags.Field.WillRetain));
	Str += "; User";
	Str += " name: " + mqtt::ToString(static_cast<bool>(VariableHeader.ConnectFlags.Field.UserNameFlag));
	Str += ", password: " + mqtt::ToString(static_cast<bool>(VariableHeader.ConnectFlags.Field.PasswordFlag));
	Str += "; Keep alive: " + std::to_string(VariableHeader.KeepAlive.Value) + " s";
	Str += "; Client ID: " + Payload.ClientId;
	Str += mqtt::ToString(", Will topic: ", Payload.WillTopic);
	Str += mqtt::ToString(", Will message: ", Payload.WillMessage);
	Str += mqtt::ToString(", User name: ", Payload.UserName);
	Str += mqtt::ToString(", Password: ", Payload.Password);
	return Str;
}

std::vector<std::uint8_t> tContentCONNECT::ToVector() const
{
	std23::vector<std::uint8_t> Data = VariableHeader.ProtocolName.ToVector();
	Data.push_back(VariableHeader.ProtocolLevel);
	Data.push_back(VariableHeader.ConnectFlags.Value);
	Data.append_range(VariableHeader.KeepAlive.ToVector());
	// These fields, if present, MUST appear in the order Client Identifier, Will Topic, Will Message, User Name, Password
	Data.append_range(Payload.ClientId.ToVector()); // The Server MUST allow ClientIds which are between 1 and 23 UTF - 8 encoded bytes in length, and that contain only the characters "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ".
	Data.append_range(mqtt::ToVector(Payload.WillTopic));
	Data.append_range(mqtt::ToVector(Payload.WillMessage));
	Data.append_range(mqtt::ToVector(Payload.UserName));
	Data.append_range(mqtt::ToVector(Payload.Password));
	Data.insert_range(Data.begin(), FixedHeader.ToVector(Data.size()));
	return Data;
}

tContentCONNECT& tContentCONNECT::operator=(tContentCONNECT&& val) noexcept
{
	if (this == &val)
		return *this;
	FixedHeader = val.FixedHeader;
	VariableHeader = std::move(val.VariableHeader);
	Payload = std::move(val.Payload);
	return *this;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CONNACK

tContentCONNACK::tContentCONNACK(bool sessionPresent, tConnectReturnCode connectRetCode)
{
	VariableHeader.ConnectAcknowledgeFlags.Field.SessionPresent = sessionPresent;
	VariableHeader.ConnectReturnCode = connectRetCode;
}

std::optional<tContentCONNACK> tContentCONNACK::Parse(tSpan& data)
{
	std::optional<std::pair<tFixedHeader, std::size_t>> FixedHeaderOpt = tFixedHeader::Parse(data);
	if (!FixedHeaderOpt.has_value() || FixedHeaderOpt->second != 2)
		return {};

	tContentCONNACK Content{};
	Content.FixedHeader = FixedHeaderOpt->first;

	Content.VariableHeader.ConnectAcknowledgeFlags.Value = data[0];
	Content.VariableHeader.ConnectReturnCode = static_cast<tConnectReturnCode>(data[1]);
	data.Skip(2);

	return Content;
}

std::string tContentCONNACK::ToString() const
{
	std::string Str = FixedHeader.ToString(true);
	Str += " ReturnCode: ";
	Str += mqtt::ToString(VariableHeader.ConnectReturnCode);

	if (VariableHeader.ConnectReturnCode != tConnectReturnCode::ConnectionAccepted)
		return Str;

	Str += "; Session Present: ";
	Str += VariableHeader.ConnectAcknowledgeFlags.Field.SessionPresent ?
		mqtt::ToString(VariableHeader.ConnectAcknowledgeFlags.Field.SessionPresent, "Continued") : mqtt::ToString(VariableHeader.ConnectAcknowledgeFlags.Field.SessionPresent, "Clean");
	return Str;
}

std::vector<std::uint8_t> tContentCONNACK::ToVector() const
{
	std23::vector<std::uint8_t> Data;
	Data.push_back(VariableHeader.ConnectAcknowledgeFlags.Value);
	Data.push_back(static_cast<std::uint8_t>(VariableHeader.ConnectReturnCode));
	Data.insert_range(Data.begin(), FixedHeader.ToVector(Data.size()));
	return Data;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLISH

tContentPUBLISH::tVariableHeader::tVariableHeader(tVariableHeader&& val) noexcept
	:PacketId(val.PacketId), TopicName(std::move(val.TopicName))
{
}

tContentPUBLISH::tVariableHeader& tContentPUBLISH::tVariableHeader::operator=(tVariableHeader&& val) noexcept
{
	if (this == &val)
		return *this;
	PacketId = val.PacketId;
	TopicName = std::move(val.TopicName);
	return *this;
}

tContentPUBLISH::tContentPUBLISH(bool dup, bool retain, const std::string& topicName)
	:FixedHeader(dup, tQoS::AtMostOnceDelivery, retain)
{
	VariableHeader.TopicName = topicName;
}

tContentPUBLISH::tContentPUBLISH(bool dup, bool retain, const std::string& topicName, const std::vector<std::uint8_t>& payload)
	:FixedHeader(dup, tQoS::AtMostOnceDelivery, retain)
{
	VariableHeader.TopicName = topicName;
	Payload = payload;
}

tContentPUBLISH::tContentPUBLISH(bool dup, bool retain, const std::string& topicName, tQoS qos, tUInt16 packetId)
	:FixedHeader(dup, qos, retain)
{
	VariableHeader.TopicName = topicName;
	VariableHeader.PacketId = packetId;
}

tContentPUBLISH::tContentPUBLISH(bool dup, bool retain, const std::string& topicName, tQoS qos, tUInt16 packetId, const std::vector<std::uint8_t>& payload)
	:FixedHeader(dup, qos, retain)
{
	VariableHeader.TopicName = topicName;
	VariableHeader.PacketId = packetId;
	Payload = payload;
}

tContentPUBLISH::tContentPUBLISH(tContentPUBLISH&& val) noexcept
	:FixedHeader(val.FixedHeader)
{
	VariableHeader = std::move(val.VariableHeader);
	Payload = std::move(val.Payload);
}

std::optional<tContentPUBLISH> tContentPUBLISH::Parse(tSpan& data)
{
	std::optional<std::pair<tFixedHeader, std::size_t>> FixedHeaderOpt = tFixedHeader::Parse(data);
	if (!FixedHeaderOpt.has_value())
		return {};

	tContentPUBLISH Content{};
	Content.FixedHeader = FixedHeaderOpt->first;

	std::optional<std::string> StrOpt = tString::Parse(data);
	if (!StrOpt.has_value())
		return {};
	Content.VariableHeader.TopicName = *StrOpt;

	if (IsPacketIdPresent(Content.FixedHeader.GetQoS()))
	{
		std::optional<tUInt16> PacketIdOpt = tUInt16::Parse(data);
		if (!PacketIdOpt.has_value())
			return {};
		Content.VariableHeader.PacketId = *PacketIdOpt;
	}
		
	// 819 The Payload contains the Application Message that is being published.The content and format of the
	// 820 data is application specific.The length of the payload can be calculated by subtracting the length of the
	// 821 variable header from the Remaining Length field that is in the Fixed Header.It is valid for a PUBLISH
	// 822 Packet to contain a zero length payload.
	Content.Payload = std::vector<std::uint8_t>(data.begin(), data.end());

	return Content;
}

std::string tContentPUBLISH::ToString() const
{
	std::string Str = FixedHeader.ToString(true);
	Str += " Topic name: " + VariableHeader.TopicName;
	Str += mqtt::ToString("; Packet ID: ", VariableHeader.PacketId);
	Str += std::string("; Payload size: ") + std::to_string(Payload.size());
	return Str;
}

std::vector<std::uint8_t> tContentPUBLISH::ToVector() const
{
	std23::vector<std::uint8_t> Data;
	Data.append_range(VariableHeader.TopicName.ToVector());
	if (VariableHeader.PacketId.has_value())
		Data.append_range(VariableHeader.PacketId->ToVector());
	Data.append_range(Payload);
	Data.insert_range(Data.begin(), FixedHeader.ToVector(Data.size()));
	return Data;
}

tContentPUBLISH& tContentPUBLISH::operator=(tContentPUBLISH&& val) noexcept
{
	if (this == &val)
		return *this;
	FixedHeader = val.FixedHeader;
	VariableHeader = std::move(val.VariableHeader);
	Payload = std::move(val.Payload);
	return *this;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SUBSCRIBE:

std::optional<tContentSUBSCRIBE::tTopicFilter> tContentSUBSCRIBE::tTopicFilter::Parse(tSpan& data)
{
	auto StrExp = tString::Parse(data);
	if (!StrExp.has_value() || data.empty())
		return {};

	tTopicFilter Parsed{};
	Parsed.TopicFilter = *StrExp;

	Parsed.QoS = static_cast<tQoS>(data[0]);
	data.Skip(1);

	return Parsed;
}

std::string tContentSUBSCRIBE::tTopicFilter::ToString() const
{
	return std::string("Topic filter: ") + TopicFilter + ", QoS: " + mqtt::ToString(QoS);
}

std::vector<std::uint8_t> tContentSUBSCRIBE::tTopicFilter::ToVector() const
{
	std::vector<std::uint8_t> Data = TopicFilter.ToVector();
	Data.push_back(static_cast<std::uint8_t>(QoS));
	return Data;
}

tContentSUBSCRIBE::tContentSUBSCRIBE(tUInt16 packetId, const payload_type& topicFilters)
{
	VariableHeader.PacketId = packetId;
	Payload = topicFilters; // [TBD] check move c_tor
}

std::optional<tContentSUBSCRIBE> tContentSUBSCRIBE::Parse(tSpan& data)
{
	std::optional<std::pair<tFixedHeader, std::size_t>> FixedHeaderOpt = tFixedHeader::Parse(data);
	if (!FixedHeaderOpt.has_value())
		return {};

	tContentSUBSCRIBE Content{};
	Content.FixedHeader = FixedHeaderOpt->first;

	std::optional<tUInt16> PacketIdOpt = tUInt16::Parse(data);
	if (!PacketIdOpt.has_value())
		return {};
	Content.VariableHeader.PacketId = *PacketIdOpt;

	do
	{
		auto TopicFilterOpt = tTopicFilter::Parse(data);
		if (!TopicFilterOpt.has_value())
			return {};
		Content.Payload.push_back(*TopicFilterOpt);
	} while (!data.empty());

	return Content;
}

std::string tContentSUBSCRIBE::ToString() const
{
	std::string Str = FixedHeader.ToString(true);
	Str += " Packet ID: " + std::to_string(VariableHeader.PacketId.Value);
	std::ranges::for_each(Payload, [&Str](const tTopicFilter& item)
		{
			if (!Str.empty())
				Str += "; ";
			Str += item.ToString();
		});
	return Str;
}

std::vector<std::uint8_t> tContentSUBSCRIBE::ToVector() const
{
	std23::vector<std::uint8_t> Data;
	Data.append_range(VariableHeader.PacketId.ToVector());
	std::ranges::for_each(Payload, [&Data](const tTopicFilter& tf) { Data.append_range(tf.ToVector()); });
	Data.insert_range(Data.begin(), FixedHeader.ToVector(Data.size()));
	return Data;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SUBACK

tContentSUBACK::tContentSUBACK(tUInt16 packetId, std::vector<tSubscribeReturnCode> payload)
{
	VariableHeader.PacketId = packetId;
	Payload = payload;
}

std::optional<tContentSUBACK> tContentSUBACK::Parse(tSpan& data)
{
	std::optional<std::pair<tFixedHeader, std::size_t>> FixedHeaderOpt = tFixedHeader::Parse(data);
	if (!FixedHeaderOpt.has_value())
		return {};

	tContentSUBACK Content{};
	Content.FixedHeader = FixedHeaderOpt->first;

	std::optional<tUInt16> PacketIdOpt = tUInt16::Parse(data);
	if (!PacketIdOpt.has_value())
		return {};
	Content.VariableHeader.PacketId = *PacketIdOpt;

	if (data.empty())
		return {};

	for (const auto i : data)
	{
		Content.Payload.push_back(static_cast<tSubscribeReturnCode>(i)); // [TBD] check - if the value is correct
		data.Skip(1);
	}

	return Content;
}

std::string tContentSUBACK::ToString() const
{
	std::string Str = FixedHeader.ToString(true);
	Str += " Packet ID: " + std::to_string(VariableHeader.PacketId.Value);
	Str += ", Return Codes:";
	std::ranges::for_each(Payload, [&Str](tSubscribeReturnCode rc) { Str += ", " + mqtt::ToString(rc); });
	return Str;
}

std::vector<std::uint8_t> tContentSUBACK::ToVector() const
{
	std23::vector<std::uint8_t> Data;
	Data.append_range(VariableHeader.PacketId.ToVector());
	std::ranges::for_each(Payload, [&Data](tSubscribeReturnCode rc) { Data.push_back(static_cast<std::uint8_t>(rc)); });
	Data.insert_range(Data.begin(), FixedHeader.ToVector(Data.size()));
	return Data;
}


/*
std::optional<tPayloadSUBACK> tPayloadSUBACK::Parse(const tVariableHeaderSUBACK& variableHeader, tSpan& data)
{
	if (data.size() != tPayloadSUBACK::GetSize())
		return {};
	tPayloadSUBACK Payload{};
	Payload.SubscribeReturnCode = static_cast<tSubscribeReturnCode>(data[0]);
	data.Skip(1);
	return Payload;
}

std::string tPayloadSUBACK::ToString() const
{
	return mqtt::ToString(SubscribeReturnCode);
}

std::vector<std::uint8_t> tPayloadSUBACK::ToVector() const
{
	std::vector<std::uint8_t> Data;
	Data.push_back(static_cast<std::uint8_t>(SubscribeReturnCode));
	return Data;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UNSUBSCRIBE: header & payload

std::optional<tPayloadUNSUBSCRIBE> tPayloadUNSUBSCRIBE::Parse(const tVariableHeaderUNSUBSCRIBE& variableHeader, tSpan& data)
{
	tPayloadUNSUBSCRIBE Payload{};
	while (!data.empty())
	{
		auto StrExp = tString::Parse(data);
		if (!StrExp.has_value())
			return {};
		Payload.TopicFilters.push_back(*StrExp);
	}
	return Payload;
}

std::size_t tPayloadUNSUBSCRIBE::GetSize() const
{
	std::size_t Size = 0;
	std::ranges::for_each(TopicFilters, [&Size](const tString& tf) { Size += tf.GetSize(); });
	return Size;
}

std::string tPayloadUNSUBSCRIBE::ToString() const
{
	std::string Str;
	for (auto& i : TopicFilters)
	{
		if (!Str.empty())
			Str += "; ";
		Str += i;
	}
	return Str;
}

std::vector<std::uint8_t> tPayloadUNSUBSCRIBE::ToVector() const
{
	std23::vector<std::uint8_t> Data;
	std::ranges::for_each(TopicFilters, [&Data](const tString& tf) { Data.append_range(tf.ToVector()); });
	return Data;
}
*/
} // hidden

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::optional<tControlPacketType> TestPacket(tSpan& data)
{
	if (data.size() < hidden::PacketSizeMin)
		return {};
	std::optional<std::pair<hidden::tFixedHeaderBase, std::size_t>> FixedHeaderOpt = hidden::tFixedHeaderBase::Parse(data);
	if (!FixedHeaderOpt.has_value())
		return {};
	data.Skip(FixedHeaderOpt->second);
	return FixedHeaderOpt->first.GetControlPacketType();
}

std::optional<tControlPacketType> TestPacket(const std::vector<std::uint8_t>& data)
{
	tSpan DataSpan(data);
	return TestPacket(DataSpan);
}

}
}
}
