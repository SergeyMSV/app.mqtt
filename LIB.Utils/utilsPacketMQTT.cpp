#include "utilsPacketMQTT.h"

namespace utils
{
namespace packet_MQTT
{

template<typename T>
typename std::enable_if<std::is_trivially_copyable<T>::value, std::string>::type ToString(T code, const std::string& msg)
{
	return std::string("[") + std::to_string(static_cast<int>(code)) + "] " + msg;
}

std::string ToString(bool state)
{
	return state ? "true" : "false";
}

std::string ToString(tControlPacketType value)
{
	switch (value)
	{
	case tControlPacketType::CONNECT: return "CONNECT";
	case tControlPacketType::CONNACK: return "CONNACK";
	case tControlPacketType::PUBLISH: return "PUBLISH";
	case tControlPacketType::PUBACK: return "PUBACK";
	case tControlPacketType::PUBREC: return "PUBREC";
	case tControlPacketType::PUBREL: return "PUBREL";
	case tControlPacketType::PUBCOMP: return "PUBCOMP";
	case tControlPacketType::SUBSCRIBE: return "SUBSCRIBE";
	case tControlPacketType::SUBACK: return "SUBACK";
	case tControlPacketType::UNSUBSCRIBE: return "UNSUBSCRIBE";
	case tControlPacketType::UNSUBACK: return "UNSUBACK";
	case tControlPacketType::PINGREQ: return "PINGREQ";
	case tControlPacketType::PINGRESP: return "PINGRESP";
	case tControlPacketType::DISCONNECT: return "DISCONNECT";
	}
	return "";
}

std::string ToString(tQoS value)
{
	switch (value)
	{
	case tQoS::AtMostOnceDelivery: return ToString(value, "At most once delivery");
	case tQoS::AtLeastOnceDelivery: return ToString(value, "At least once delivery");
	case tQoS::ExactlyOnceDelivery: return ToString(value, "Exactly once delivery");
	}
	return ToString(value, "ERROR");
}

std::string ToString(tConnectReturnCode value)
{
	switch (value)
	{
	case tConnectReturnCode::ConnectionAccepted: return ToString(value, "Connection Accepted");
	case tConnectReturnCode::ConnectionRefused_UnacceptableProtocolVersion: return ToString(value, "Connection Refused, unacceptable protocol version");
	case tConnectReturnCode::ConnectionRefused_IdentifierRejected: return ToString(value, "Connection Refused, identifier rejected");
	case tConnectReturnCode::ConnectionRefused_ServerUnavailable: return ToString(value, "Connection Refused, Server unavailable");
	case tConnectReturnCode::ConnectionRefused_BadUserNameOrPassword: return ToString(value, "Connection Refused, bad user name or password");
	case tConnectReturnCode::ConnectionRefused_NotAuthorized: return ToString(value, "Connection Refused, not authorized");
	}
	return ToString(value, "ERROR");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::expected<tUInt16, tError> tUInt16::Parse(tSpan& data)
{
	if (data.size() < 2)
		return std::unexpected(tError::UInt16TooShort);
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

tUInt16& tUInt16::operator=(std::uint16_t value)
{
	Value = value;
	return *this;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::expected<tString, tError> tString::Parse(tSpan& data)
{
	auto ClientIdLengthExp = tUInt16::Parse(data);
	if (!ClientIdLengthExp.has_value())
		return std::unexpected(ClientIdLengthExp.error());

	if (data.size() < ClientIdLengthExp->Value)
		return std::unexpected(tError::StringTooShort);

	std::string Str(data.begin(), data.begin() + ClientIdLengthExp->Value);

	data.Skip(ClientIdLengthExp->Value);

	return Str;
}

std::vector<std::uint8_t> tString::ToVector() const
{
	std::vector<std::uint8_t> Data;
	tUInt16 StrSize = static_cast<std::uint8_t>(size());
	Data.append_range(StrSize.ToVector());
	Data.insert(Data.end(), begin(), end());
	return Data;
}

namespace hidden
{

tRemainingLengthParseExp tRemainingLength::Parse(tSpan& data)
{
	if (data.empty())
		return std::unexpected(tError::LengthTooShort);

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

	return std::unexpected(data.size() < m_SizeMax ? tError::LengthNotAll : tError::LengthTooLong);
}

tRemainingLengthToVectorExp tRemainingLength::ToVector(std::uint32_t value)
{
	std::vector<std::uint8_t> Data;
	for (int i = 0; i < m_SizeMax; ++i)
	{
		tLengthPart Part{};
		Part.Field.Num = value;

		value = value >> 7;
		if (value)
			Part.Field.Continuation = 1;

		Data.push_back(Part.Value);

		if (!value)
			break;
	}

	if (value)
		return std::unexpected(tError::LengthOverflow);

	return Data;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CONNECT: header & payload

std::expected<tVariableHeaderCONNECT, tError> tVariableHeaderCONNECT::Parse(const hidden::tFixedHeader& fixedHeader, tSpan& data)
{
	auto ProtocolNameExp = tString::Parse(data);
	if (!ProtocolNameExp.has_value())
		return std::unexpected(ProtocolNameExp.error());

	constexpr std::size_t VHeaderSize = sizeof(ProtocolLevel) + sizeof(ConnectFlags) + sizeof(KeepAlive);
	if (VHeaderSize > data.size())
		return std::unexpected(tError::VariableHeaderTooShort);

	tVariableHeaderCONNECT VHeader{};
	VHeader.ProtocolName = *ProtocolNameExp;
	VHeader.ProtocolLevel = data[0];
	VHeader.ConnectFlags.Value = data[1];
	data.Skip(2);
	auto KeepAliveExp = tUInt16::Parse(data);
	if (!KeepAliveExp.has_value())
		return std::unexpected(KeepAliveExp.error());
	VHeader.KeepAlive = *KeepAliveExp;

	return VHeader;
}

std::string tVariableHeaderCONNECT::ToString() const
{
	std::string Res("Protocol");
	Res += " name: " + ProtocolName;
	Res += ", level: " + std::to_string(ProtocolLevel);
	Res += "; Clean session: " + packet_MQTT::ToString(ConnectFlags.Field.CleanSession);
	Res += "; Will";
	Res += " flag: " + packet_MQTT::ToString(ConnectFlags.Field.WillFlag);
	Res += ", QoS: " + packet_MQTT::ToString(static_cast<tQoS>(ConnectFlags.Field.WillQoS));
	Res += ", retain: " + packet_MQTT::ToString(ConnectFlags.Field.WillRetain);
	Res += "; User";
	Res += " name: " + packet_MQTT::ToString(ConnectFlags.Field.UserNameFlag);
	Res += ", password: " + packet_MQTT::ToString(ConnectFlags.Field.PasswordFlag);
	Res += "; Keep alive: " + std::to_string(KeepAlive.Value) + " s.";
	return Res;
}

std::vector<std::uint8_t> tVariableHeaderCONNECT::ToVector() const
{
	std::vector<std::uint8_t> Data = ProtocolName.ToVector();
	Data.push_back(ProtocolLevel);
	Data.push_back(ConnectFlags.Value);
	Data.append_range(KeepAlive.ToVector());
	return Data;
}

bool tVariableHeaderCONNECT::operator==(const tVariableHeaderCONNECT& val) const
{
	return ProtocolName == val.ProtocolName && ProtocolLevel == val.ProtocolLevel && ConnectFlags.Value == val.ConnectFlags.Value && KeepAlive.Value == val.KeepAlive.Value;
}

std::expected<tPayloadCONNECT, tError> tPayloadCONNECT::Parse(const tVariableHeaderCONNECT& variableHeader, tSpan& data)
{
	tPayloadCONNECT Payload{};
	// These fields, if present, MUST appear in the order Client Identifier, Will Topic, Will Message, User Name, Password
	auto StrExp = tString::Parse(data);
	if (!StrExp.has_value())
		return std::unexpected(StrExp.error());
	Payload.ClientId = StrExp.value();

	if (variableHeader.ConnectFlags.Field.WillFlag)
	{
		StrExp = tString::Parse(data);
		if (!StrExp.has_value())
			return std::unexpected(StrExp.error());
		Payload.WillTopic = StrExp.value();

		StrExp = tString::Parse(data);
		if (!StrExp.has_value())
			return std::unexpected(StrExp.error());
		Payload.WillMessage = StrExp.value();
	}

	if (variableHeader.ConnectFlags.Field.UserNameFlag)
	{
		StrExp = tString::Parse(data);
		if (!StrExp.has_value())
			return std::unexpected(StrExp.error());
		Payload.UserName = StrExp.value();

		StrExp = tString::Parse(data);
		if (!StrExp.has_value())
			return std::unexpected(StrExp.error());
		Payload.Password = StrExp.value();
	}

	return Payload;
}

std::vector<std::uint8_t> tPayloadCONNECT::ToVector() const
{
	// These fields, if present, MUST appear in the order Client Identifier, Will Topic, Will Message, User Name, Password
	std::vector<std::uint8_t> Data = ClientId.ToVector(); // The Server MUST allow ClientIds which are between 1 and 23 UTF - 8 encoded bytes in length, and that contain only the characters "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ".
	if (WillTopic.has_value())
		Data.append_range(WillTopic->ToVector());
	if (WillMessage.has_value())
		Data.append_range(WillMessage->ToVector());
	if (UserName.has_value())
		Data.append_range(UserName->ToVector());
	if (Password.has_value())
		Data.append_range(Password->ToVector());
	return Data;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CONNACK: header & payload

std::expected<tVariableHeaderCONNACK, tError> tVariableHeaderCONNACK::Parse(const hidden::tFixedHeader& fixedHeader, tSpan& data)
{
	if (data.size() < GetSize())
		return std::unexpected(tError::VariableHeaderTooShort);

	tVariableHeaderCONNACK VHeader{};
	VHeader.ConnectAcknowledgeFlags.Value = data[0];
	VHeader.ConnectReturnCode = static_cast<tConnectReturnCode>(data[1]);

	data.Skip(GetSize());

	return VHeader;
}

std::string tVariableHeaderCONNACK::ToString() const
{
	std::string Res("ReturnCode: ");
	Res += packet_MQTT::ToString(ConnectReturnCode);

	if (ConnectReturnCode != tConnectReturnCode::ConnectionAccepted)
		return Res;

	Res += "; Session Present: ";
	Res += ConnectAcknowledgeFlags.Field.SessionPresent ?
			packet_MQTT::ToString(ConnectAcknowledgeFlags.Field.SessionPresent, "Continued") : packet_MQTT::ToString(ConnectAcknowledgeFlags.Field.SessionPresent, "Clean");
	return Res;
}

std::vector<std::uint8_t> tVariableHeaderCONNACK::ToVector() const
{
	std::vector<std::uint8_t> Data;
	Data.push_back(ConnectAcknowledgeFlags.Value);
	Data.push_back(static_cast<std::uint8_t>(ConnectReturnCode));
	return Data;
}

bool tVariableHeaderCONNACK::operator==(const tVariableHeaderCONNACK& val) const
{
	return ConnectAcknowledgeFlags.Value == val.ConnectAcknowledgeFlags.Value && ConnectReturnCode == val.ConnectReturnCode;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLISH: header & payload

std::expected<tVariableHeaderPUBLISH, tError> tVariableHeaderPUBLISH::Parse(const hidden::tFixedHeader& fixedHeader, tSpan& data)
{
	auto StrExp = tString::Parse(data);
	if (!StrExp.has_value())
		return std::unexpected(StrExp.error());

	tVariableHeaderPUBLISH VHeader{};
	VHeader.TopicName = *StrExp;

	if (!tPacketPUBLISH::IsPacketIdPresent(fixedHeader.Field.Flags))
		return VHeader;

	auto PacketIdExp = tUInt16::Parse(data);
	if (!PacketIdExp.has_value())
		return std::unexpected(PacketIdExp.error());

	VHeader.PacketId = *PacketIdExp;

	return VHeader;
}

std::vector<std::uint8_t> tVariableHeaderPUBLISH::ToVector() const
{
	std::vector<std::uint8_t> Data = TopicName.ToVector();
	if (PacketId.has_value())
		Data.append_range(PacketId->ToVector());
	return Data;
}

bool tVariableHeaderPUBLISH::operator==(const tVariableHeaderPUBLISH& val) const
{
	return
		TopicName == val.TopicName &&
		(PacketId.has_value() != val.PacketId.has_value() ? false : PacketId.has_value() == false ? true : *PacketId == *val.PacketId);
}

std::expected<tPayloadPUBLISH, tError> tPayloadPUBLISH::Parse(const tVariableHeaderPUBLISH& variableHeader, tSpan& data)
{
	// 819 The Payload contains the Application Message that is being published.The content and format of the
	// 820 data is application specific.The length of the payload can be calculated by subtracting the length of the
	// 821 variable header from the Remaining Length field that is in the Fixed Header.It is valid for a PUBLISH
	// 822 Packet to contain a zero length payload.
	tPayloadPUBLISH Payload{};
	Payload.Data = std::vector<std::uint8_t>(data.begin(), data.end());
	return Payload;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PUBACK: header & payload
std::expected<tVariableHeaderPUBACK, tError> tVariableHeaderPUBACK::Parse(const hidden::tFixedHeader& fixedHeader, tSpan& data)
{
	auto PacketIdExp = tUInt16::Parse(data);
	if (!PacketIdExp.has_value())
		return std::unexpected(PacketIdExp.error());

	tVariableHeaderPUBACK VHeader{};
	VHeader.PacketId = *PacketIdExp;

	return VHeader;
}

} // hidden


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

tPacketCONNECT::tPacketCONNECT(bool cleanSession, std::uint16_t keepAlive, const std::string& clientId, tQoS willQos, bool willRetain, const std::string& willTopic, const std::string& willMessage, const std::string& userName, const std::string& password)
	:tPacket(GetFixedHeader())
{
	m_VariableHeader = hidden::tVariableHeaderCONNECT{};
	m_VariableHeader->ConnectFlags.Field.WillQoS = 1; // [TBD] TEST
	m_VariableHeader->ConnectFlags.Field.CleanSession = cleanSession ? 1 : 0;
	m_VariableHeader->KeepAlive.Value = keepAlive;

	m_Payload = hidden::tPayloadCONNECT{};

	SetClientId(clientId);
	SetWill(willQos, willRetain, willTopic, willMessage);
	SetUser(userName, password);
}


void tPacketCONNECT::SetClientId(std::string value)
{
	// 579 The Server MUST allow ClientIds which are between 1 and 23 UTF-8 encoded bytes in length, and that
	// 580 contain only the characters
	// 581 "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ" [MQTT-3.1.3-5].
	constexpr std::size_t ClientIdSizeMax = 23;
	if (value.size() > ClientIdSizeMax)
		value.resize(ClientIdSizeMax);
	// 583 The Server MAY allow ClientId�s that contain more than 23 encoded bytes. The Server MAY allow
	// 584 ClientId�s that contain characters not included in the list given above.
	// 
	// 586 A Server MAY allow a Client to supply a ClientId that has a length of zero bytes, however if it does so the
	// 587 Server MUST treat this as a special case and assign a unique ClientId to that Client. It MUST then
	// 588 process the CONNECT packet as if the Client had provided that unique ClientId [MQTT-3.1.3-6].
	// 
	// 590 If the Client supplies a zero-byte ClientId, the Client MUST also set CleanSession to 1 [MQTT-3.1.3-7].
	m_Payload->ClientId = value;
}

void tPacketCONNECT::SetWill(tQoS qos, bool retain, const std::string& topic, const std::string& message)
{
	m_VariableHeader->ConnectFlags.Field.WillFlag = !topic.empty() && !message.empty();

	if (m_VariableHeader->ConnectFlags.Field.WillFlag)
	{
		m_VariableHeader->ConnectFlags.Field.WillQoS = static_cast<std::uint8_t>(qos);
		m_VariableHeader->ConnectFlags.Field.WillRetain = retain ? 1 : 0;
		m_Payload->WillTopic = topic;
		m_Payload->WillMessage = message;
	}
	else
	{
		m_VariableHeader->ConnectFlags.Field.WillQoS = 0; // 510 If the Will Flag is set to 0, then the Will QoS MUST be set to 0 (0x00) [MQTT-3.1.2-13].
		m_VariableHeader->ConnectFlags.Field.WillRetain = 0; // 518 If the Will Flag is set to 0, then the Will Retain Flag MUST be set to 0[MQTT - 3.1.2 - 15].
		m_Payload->WillTopic.reset();
		m_Payload->WillMessage.reset();
	}
}

void tPacketCONNECT::SetUser(const std::string& name, const std::string& password)
{
	// 532 If the Password Flag is set to 0, a password MUST NOT be present in the payload [MQTT-3.1.2-20].
	// 532 If the Password Flag is set to 1, a password MUST be present in the payload [MQTT-3.1.2-21].
	// 533 If the User Name Flag is set to 0, the Password Flag MUST be set to 0 [MQTT-3.1.2-22].
	m_VariableHeader->ConnectFlags.Field.UserNameFlag = !name.empty();
	m_VariableHeader->ConnectFlags.Field.PasswordFlag = !name.empty() && !password.empty();

	if (m_VariableHeader->ConnectFlags.Field.UserNameFlag)
	{
		m_Payload->UserName = name;
	}
	else
	{
		m_Payload->UserName.reset();
	}

	if (m_VariableHeader->ConnectFlags.Field.PasswordFlag)
	{
		m_Payload->Password = password;
	}
	else
	{
		m_Payload->Password.reset();
	}
}

tPacketPUBLISH::tPacketPUBLISH(bool dup, bool retain, const std::string& topicName, tQoS qos, tUInt16 packetId)
	:tPacket(GetFixedHeader(dup, qos, retain))
{
	m_VariableHeader = hidden::tVariableHeaderPUBLISH{};
	m_VariableHeader->TopicName = topicName;
	if (IsPacketIdPresent(m_FixedHeader.Field.Flags))
		m_VariableHeader->PacketId = packetId;
	//else - error: no PacketId
}

tPacketPUBLISH::tPacketPUBLISH(bool dup, bool retain, const std::string& topicName, tQoS qos, tUInt16 packetId, const std::vector<std::uint8_t>& payloadData)
	:tPacketPUBLISH(dup, retain, topicName, qos, packetId)
{
	m_Payload = hidden::tPayloadPUBLISH{};
	m_Payload->Data = payloadData;
}

tPacketPUBLISH::tPacketPUBLISH(bool dup, bool retain, const std::string& topicName)
	:tPacket(GetFixedHeader(dup, tQoS::AtMostOnceDelivery, retain))
{
	m_VariableHeader = hidden::tVariableHeaderPUBLISH{};
	m_VariableHeader->TopicName = topicName;
}

tPacketPUBLISH::tPacketPUBLISH(bool dup, bool retain, const std::string& topicName, const std::vector<std::uint8_t>& payloadData)
	:tPacketPUBLISH(dup, retain, topicName)
{
	m_Payload = hidden::tPayloadPUBLISH{};
	m_Payload->Data = payloadData;
}

bool tPacketPUBLISH::IsPacketIdPresent(std::uint8_t flags)
{
	hidden::tFixedHeaderPUBLISHFlags Flags;
	Flags.Value = flags;
	return static_cast<tQoS>(Flags.Field.QoS) == tQoS::AtLeastOnceDelivery || static_cast<tQoS>(Flags.Field.QoS) == tQoS::ExactlyOnceDelivery;
}

hidden::tFixedHeader tPacketPUBLISH::GetFixedHeader(bool dup, tQoS qos, bool retain)
{
	hidden::tFixedHeaderPUBLISHFlags Flags;

	Flags.Field.DUP = dup ? 1 : 0;
	Flags.Field.QoS = static_cast<std::uint8_t>(qos);
	Flags.Field.RETAIN = retain ? 1 : 0;

	return hidden::MakeFixedHeader(tControlPacketType::PUBLISH, Flags.Value);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::expected<tControlPacketType, tError> TestPacket(tSpan& data)
{
	if (data.size() < hidden::PacketSizeMin)
		return std::unexpected(tError::PacketTooShort);

	hidden::tFixedHeader FHeader = data[0];
	data.Skip(1);

	const auto ControlPacketType = static_cast<tControlPacketType>(FHeader.Field.ControlPacketType);
	if (ControlPacketType < tControlPacketType::CONNECT || ControlPacketType > tControlPacketType::DISCONNECT)
		return std::unexpected(tError::PacketType);

	auto RLengtExp = hidden::tRemainingLength::Parse(data);
	if (!RLengtExp.has_value())
		return std::unexpected(RLengtExp.error());
	if (*RLengtExp > data.size())
		return std::unexpected(tError::PacketTooShort);

	return FHeader.GetControlPacketType();
}

std::expected<tControlPacketType, tError> TestPacket(const std::vector<std::uint8_t>& data)
{
	tSpan DataSpan(data);
	return TestPacket(DataSpan);
}

}
}
