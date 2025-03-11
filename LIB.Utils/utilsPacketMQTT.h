///////////////////////////////////////////////////////////////////////////////////////////////////
// utilsPacketMQTT
// 2024-11-22
// C++23
// 
// Specification: mqtt-v3.1.1.pdf (MQTT Version 3.1.1 Plus Errata 01; OASIS Standard Incorporating Approved Errata 01; 10 December 2015)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <libConfig.h>

#include <expected> // C++ 23
#include <optional>
#include <span> // C++ 20
#include <string>
#include <vector>

#include <cstdint>

namespace utils
{
namespace packet_MQTT
{
// 178 Text fields in the Control Packets described later are encoded as UTF-8 strings. ...
// 187 Unless stated otherwise all UTF-8 encoded strings can have any length in the range 0 to 65535 bytes.

// 190 The character data in a UTF-8 encoded string MUST be well-formed UTF-8 as defined by the Unicode
// 191 specification [Unicode] and restated in RFC 3629 [RFC3629]. In particular this data MUST NOT include
// 192 encodings of code points between U+D800 and U+DFFF. If a Server or Client receives a Control Packet
// 193 containing ill-formed UTF-8 it MUST close the Network Connection [MQTT-1.5.3-1].

// 195 A UTF-8 encoded string MUST NOT include an encoding of the null character U + 0000. If a receiver
// 196 (Server or Client) receives a Control Packet containing U + 0000 it MUST close the Network
// 197 Connection[MQTT-1.5.3-2].

// 207 A UTF-8 encoded sequence 0xEF 0xBB 0xBF is always to be interpreted to mean U+FEFF ("ZERO
// 208 WIDTH NO-BREAK SPACE") wherever it appears in a string and MUST NOT be skipped over or stripped
// 209 off by a packet receiver [MQTT-1.5.3-3].

enum class tControlPacketType
{
	CONNECT = 1,	// Client to Server							Client request to connect to Server
	CONNACK,		// Server to Client							Connect acknowledgment
	PUBLISH,		// Client to Server or Server to Client		Publish message
	PUBACK,			// Client to Server or Server to Client		Publish acknowledgment
	PUBREC,			// Client to Server or Server to Client		Publish received (assured delivery part 1)
	PUBREL,			// Client to Server or Server to Client		Publish release (assured delivery part 2)
	PUBCOMP,		// Client to Server or Server to Client		Publish complete (assured delivery part 3)
	SUBSCRIBE,		// Client to Server							Client subscribe request
	SUBACK,			// Server to Client							Subscribe acknowledgment
	UNSUBSCRIBE,	// Client to Server							Unsubscribe request
	UNSUBACK,		// Server to Client							Unsubscribe acknowledgment
	PINGREQ,		// Client to Server							PING request
	PINGRESP,		// Server to Client							PING response
	DISCONNECT,		// Client to Server							Client is disconnecting
};

enum class tError
{
	None,
	WrongSize,
	LengthTooShort,
	LengthTooLong,
	LengthNotAll, // There are not all of the bytes of the Length in the array.
	LengthOverflow,
	UInt16TooShort,
	StringTooShort,
	PacketTooShort,
	PacketType,
	VariableHeaderTooShort,
	PayloadTooShort,
	TopicFilterTooShort,
	//Format,
	ProtocolName,
	ProtocolLevel,
};

enum class tQoS : std::uint8_t
{
	AtMostOnceDelivery,
	AtLeastOnceDelivery,
	ExactlyOnceDelivery,
};

enum class tConnectReturnCode : std::uint8_t
{
	ConnectionAccepted,
	ConnectionRefused_UnacceptableProtocolVersion, // The Server does not support the level of the MQTT protocol requested by the Client.
	ConnectionRefused_IdentifierRejected, // The Client identifier is correct UTF-8 but not allowed by the Server.
	ConnectionRefused_ServerUnavailable, // The Network Connection has been made but the MQTT service is unavailable.
	ConnectionRefused_BadUserNameOrPassword, // The data in the user name or password is malformed.
	ConnectionRefused_NotAuthorized, // The Client is not authorized to connect.
};

enum class tSubscribeReturnCode : std::uint8_t
{
	SuccessMaximumQoS_AtMostOnceDelivery = tQoS::AtMostOnceDelivery,
	SuccessMaximumQoS_AtLeastOnceDelivery = tQoS::AtLeastOnceDelivery,
	SuccessMaximumQoS_ExactlyOnceDelivery = tQoS::ExactlyOnceDelivery,
	Failure = 0x80,
};

template<typename T> std::string ToString(T value);

class tSpan : public std::span<const std::uint8_t>
{
public:
	tSpan(const std::vector<std::uint8_t>& data) :std::span<const std::uint8_t>(data) {}
	tSpan(const std::vector<std::uint8_t>& data, std::size_t size) :std::span<const std::uint8_t>(data.begin(), std::min(size, data.size())) {}
	tSpan(tSpan& data, std::size_t size) :std::span<const std::uint8_t>(data.begin(), std::min(size, data.size())) {}

	tSpan& operator=(const tSpan& value) = default;

	void Skip(std::size_t count)
	{
		if (count >= size())
			count = size();
		*static_cast<std::span<const std::uint8_t>*>(this) = subspan(count, size() - count);
	}
};

#pragma pack(push, 1)
union tUInt16
{
	struct
	{
		std::uint16_t LSB : 8;
		std::uint16_t MSB : 8;
	}Field;
	std::uint16_t Value = 0;

	tUInt16() = default;
	tUInt16(std::uint16_t value) :Value(value) {} // not explicit

	static constexpr std::size_t GetSize() { return 2; }

	static std::expected<tUInt16, tError> Parse(tSpan& data);

	std::vector<std::uint8_t> ToVector() const;

	tUInt16& operator=(std::uint16_t value);
	bool operator==(const tUInt16& val) const { return Value == val.Value; }
};
#pragma pack(pop)

class tString : public std::string
{
public:
	tString() = default;
	tString(const char* value) :std::string(value) {} // not explicit
	tString(const std::string& value) :std::string(value) {} // not explicit
	tString(std::string&& value) :std::string(value) {} // not explicit

	std::size_t GetSize() const { return size() + GetSizeMin(); }
	static constexpr std::size_t GetSizeMin() { return tUInt16::GetSize(); }

	static std::expected<tString, tError> Parse(tSpan& data);

	std::vector<std::uint8_t> ToVector() const;
};

namespace hidden
{

// MQTT 3.1.1
constexpr char DefaultProtocolName[] = "MQTT"; // 397 The string, its offset and length will not be changed by future versions of the MQTT specification.
constexpr std::uint8_t DefaultProtocolLevel = 4; // 408 ... The value of the Protocol Level field for the version 3.1.1 of the protocol is 4 (0x04).

// MQTT 3.1
//constexpr char DefaultProtocolName[] = "MQIsdp";
//constexpr std::uint8_t DefaultProtocolLevel = 3;

constexpr std::size_t PacketSizeMin = 2; // The minimum packet size is 2 bytes
constexpr std::size_t PacketSizeMax = 256 * 1024 * 1024; // The maximum packet size is 256 MB.

union tFixedHeader
{
	struct
	{
		std::uint8_t Flags : 4;
		std::uint8_t ControlPacketType : 4;
	}Field;
	std::uint8_t Value = 0;

	tFixedHeader() = default;
	tFixedHeader(std::uint8_t value) :Value(value) {} // not explicit

	tControlPacketType GetControlPacketType() const { return static_cast<tControlPacketType>(Field.ControlPacketType); }

	std::string ToString() const;

	bool operator==(const tFixedHeader& val) const { return Value == val.Value; }
};

constexpr tFixedHeader MakeFixedHeader(tControlPacketType type, std::uint8_t flags)
{
	tFixedHeader Header{};
	Header.Field.ControlPacketType = static_cast<std::uint8_t>(type);
	Header.Field.Flags = flags;
	return Header;
}

constexpr tFixedHeader MakeFixedHeader(tControlPacketType type)
{
	std::uint8_t Flags = 0;
	if (type == tControlPacketType::PUBREL ||
		type == tControlPacketType::SUBSCRIBE ||
		type == tControlPacketType::UNSUBSCRIBE)
	{
		Flags = 0x02;
	}
	return MakeFixedHeader(type, Flags);
}

using tRemainingLengthParseExp = std::expected<std::uint32_t, tError>;
using tRemainingLengthToVectorExp = std::expected<std::vector<std::uint8_t>, tError>;

class tRemainingLength
{
	static constexpr std::size_t m_SizeMax = 4;

	union tLengthPart
	{
		struct
		{
			std::uint8_t Num : 7;
			std::uint8_t Continuation : 1;
		}Field;
		std::uint8_t Value = 0;
	};

	tRemainingLength() = delete;

public:
	static tRemainingLengthParseExp Parse(tSpan& data);
	static tRemainingLengthToVectorExp ToVector(std::uint32_t value);
};

template <class VH, class PL>
class tPacket
{
protected:
	tFixedHeader m_FixedHeader{};

	std::optional<VH> m_VariableHeader;
	std::optional<PL> m_Payload;

private:
	tPacket() = default;

protected:
	explicit tPacket(tFixedHeader fixedHeader) :m_FixedHeader(fixedHeader) {}

public:
	virtual ~tPacket() {}

	std::optional<VH> GetVariableHeader() { return m_VariableHeader; }
	std::optional<PL> GetPayload() { return m_Payload; }

	static std::expected<tPacket, tError> Parse(tSpan& data)
	{
		if (data.empty())
			return std::unexpected(tError::PacketTooShort);

		tFixedHeader FHeader = data[0];

		const auto ControlPacketType = static_cast<tControlPacketType>(FHeader.Field.ControlPacketType);
		if (ControlPacketType < tControlPacketType::CONNECT || ControlPacketType > tControlPacketType::DISCONNECT)
			return std::unexpected(tError::PacketType);

		tPacket Pack{};
		Pack.m_FixedHeader = FHeader;
		data.Skip(1);

		auto RLengtExp = tRemainingLength::Parse(data);
		if (!RLengtExp.has_value())
			return std::unexpected(RLengtExp.error());
		if (*RLengtExp > data.size())
			return std::unexpected(tError::PacketTooShort);
		
		tSpan PacketDataSpan(data, *RLengtExp);

		switch (ControlPacketType)
		{
		case tControlPacketType::PINGREQ:
		case tControlPacketType::PINGRESP:
		case tControlPacketType::DISCONNECT:
			return Pack;
		}

		auto VarHeadExp = VH::Parse(Pack.m_FixedHeader, PacketDataSpan);
		if (!VarHeadExp.has_value())
			return std::unexpected(VarHeadExp.error());
		Pack.m_VariableHeader = *VarHeadExp;
		
		switch (ControlPacketType)
		{
		case tControlPacketType::CONNACK:
		case tControlPacketType::PUBACK:
		case tControlPacketType::PUBREC:
		case tControlPacketType::PUBREL:
		case tControlPacketType::PUBCOMP:
		case tControlPacketType::UNSUBACK:
		case tControlPacketType::PINGREQ:
		case tControlPacketType::PINGRESP:
			return Pack;
		case tControlPacketType::PUBLISH:
			if (PacketDataSpan.empty())
				return Pack;
			break;
		}

		if (ControlPacketType == tControlPacketType::CONNECT ||
			ControlPacketType == tControlPacketType::PUBLISH && !PacketDataSpan.empty() ||
			ControlPacketType == tControlPacketType::SUBSCRIBE ||
			ControlPacketType == tControlPacketType::SUBACK ||
			ControlPacketType == tControlPacketType::UNSUBSCRIBE)
		{
			auto PayloadExp = PL::Parse(*Pack.m_VariableHeader, PacketDataSpan);
			if (!PayloadExp.has_value())
				return std::unexpected(PayloadExp.error());
			Pack.m_Payload = *PayloadExp;
		}

		return Pack;
	}

	static std::expected<tPacket, tError> Parse(const std::vector<std::uint8_t>& data)
	{
		tSpan DataSpan(data);
		return Parse(DataSpan);
	}

	std::string ToString() const
	{
		std::string Str = m_FixedHeader.ToString();
		if (m_VariableHeader.has_value())
			Str += "; - " + m_VariableHeader->ToString();
		if (m_Payload.has_value())
			Str += "; - " + m_Payload->ToString();
		return Str;
	}

	std::vector<std::uint8_t> ToVector() const
	{
		std::vector<std::uint8_t> Data;
		if (m_VariableHeader.has_value())
			Data.append_range(m_VariableHeader->ToVector());
		if (m_Payload.has_value())
			Data.append_range(m_Payload->ToVector());
		auto RemainingLength = tRemainingLength::ToVector(static_cast<std::uint32_t>(Data.size()));
		if (!RemainingLength.has_value())
			return {};
		Data.insert_range(Data.begin(), *RemainingLength);
		Data.insert(Data.begin(), m_FixedHeader.Value);
		return Data;
	}

	bool operator==(const tPacket& val) const
	{
		return m_FixedHeader == val.m_FixedHeader && m_VariableHeader == val.m_VariableHeader && m_Payload == val.m_Payload;
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct tVariableHeaderEmpty
{
	static std::expected<tVariableHeaderEmpty, tError> Parse(const hidden::tFixedHeader& fixedHeader, tSpan& data) { return tVariableHeaderEmpty(); }

	std::string ToString() const { return{}; }

	std::vector<std::uint8_t> ToVector() const { return {}; }

	bool operator==(const tVariableHeaderEmpty& value) const = default;
};

template <class VH>
struct tPayloadEmpty
{
	static std::expected<tPayloadEmpty, tError> Parse(const VH& variableHeader, tSpan& data) { return tPayloadEmpty(); }

	std::string ToString() const { return{}; }

	std::vector<std::uint8_t> ToVector() const { return {}; }

	bool operator==(const tPayloadEmpty& value) const = default;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CONNECT

struct tVariableHeaderCONNECT
{
	tString ProtocolName;
	std::uint8_t ProtocolLevel;

	union tConnectFlags
	{
		struct
		{
			std::uint8_t Reserved : 1;
			std::uint8_t CleanSession : 1;
			std::uint8_t WillFlag : 1;
			std::uint8_t WillQoS : 2;
			// Normally if a publisher publishes a message to a topic, and no one is subscribed to that topic the message is simply discarded by the broker.
			// However the publisher can tell the broker to keep the last message on that topic by setting the retained message flag.
			std::uint8_t WillRetain : 1; 
			std::uint8_t PasswordFlag : 1;
			std::uint8_t UserNameFlag : 1;
		}Field;
		std::uint8_t Value = 0;
	}ConnectFlags;

	// 538 The Keep Alive is a time interval measured in seconds.
	// 
	// 538 If the Keep Alive value is non-zero and the Server does not receive a Control Packet from the Client
	// 547 within one and a half times the Keep Alive time period, it MUST disconnect the Network Connection to the
	// 548 Client as if the network had failed [MQTT-3.1.2-24].
	// 
	// 560 ... The maximum value is 18 hours 12 minutes and 15 seconds.
	tUInt16 KeepAlive = 0;

	tVariableHeaderCONNECT() :ProtocolName(hidden::DefaultProtocolName), ProtocolLevel(hidden::DefaultProtocolLevel) {}

	static std::expected<tVariableHeaderCONNECT, tError> Parse(const hidden::tFixedHeader& fixedHeader, tSpan& data);

	std::size_t GetSize() const { return ProtocolName.GetSize() + sizeof(ProtocolLevel) + sizeof(ConnectFlags) + tUInt16::GetSize(); }

	std::string ToString() const;

	std::vector<std::uint8_t> ToVector() const;

	bool operator==(const tVariableHeaderCONNECT& val) const;
};

struct tPayloadCONNECT
{
	// 566 The payload of the CONNECT Packet contains one or more length - prefixed fields, whose presence is
	// 567 determined by the flags in the variable header.

	// 570 The Client Identifier (ClientId) identifies the Client to the Server. 
	//
	// 579 The Server MUST allow ClientIds which are between 1 and 23 UTF-8 encoded bytes in length, and that
	// 580 contain only the characters
	// 581 "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ" [MQTT-3.1.3-5].
	tString ClientId;

	// 567 ... These fields, if present, MUST appear in the order Client
	// 568 Identifier, Will Topic, Will Message, User Name, Password [MQTT-3.1.3-1].

	// 483 If the Will Flag is set to 1 this indicates that, if the Connect request is accepted, a Will Message MUST be
	// 484 stored on the Server and associated with the Network Connection.The Will Message MUST be published
	// 485 when the Network Connection is subsequently closed unless the Will Message has been deleted by the
	// 486 Server on receipt of a DISCONNECT Packet[MQTT - 3.1.2 - 8].
	std::optional<tString> WillTopic;
	std::optional<tString> WillMessage;

	std::optional<tString> UserName;
	std::optional<tString> Password;

	static std::expected<tPayloadCONNECT, tError> Parse(const tVariableHeaderCONNECT& variableHeader, tSpan& data);

	std::string ToString() const;

	std::vector<std::uint8_t> ToVector() const;

	bool operator==(const tPayloadCONNECT& value) const = default;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CONNACK

struct tVariableHeaderCONNACK
{
	union tConnectAcknowledgeFlags
	{
		struct
		{
			std::uint8_t SessionPresent : 1; // 683 ... the value set in Session Present depends on whether the Server already has stored Session state for the supplied client ID.
			std::uint8_t Reserved : 7;
		}Field;
		std::uint8_t Value = 0;
	}ConnectAcknowledgeFlags;
	tConnectReturnCode ConnectReturnCode = tConnectReturnCode::ConnectionAccepted;

	tVariableHeaderCONNACK() = default;

	static std::expected<tVariableHeaderCONNACK, tError> Parse(const hidden::tFixedHeader& fixedHeader, tSpan& data);

	static std::size_t GetSize() { return sizeof(ConnectAcknowledgeFlags) + sizeof(ConnectReturnCode); }

	std::string ToString() const;

	std::vector<std::uint8_t> ToVector() const;

	bool operator==(const tVariableHeaderCONNACK& val) const;
};

using tPayloadCONNACK = tPayloadEmpty<tVariableHeaderCONNACK>;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLISH

union tFixedHeaderPUBLISHFlags
{
	struct
	{
		// Normally if a publisher publishes a message to a topic, and no one is subscribed to that topic the message is simply discarded by the broker.
		// However the publisher can tell the broker to keep the last message on that topic by setting the retained message flag.
		std::uint8_t RETAIN : 1;
		std::uint8_t QoS : 2;
		std::uint8_t DUP : 1;
		std::uint8_t : 4;
	}Field;
	std::uint8_t Value = 0;
};

struct tVariableHeaderPUBLISH
{
	tString TopicName;
	std::optional<tUInt16> PacketId; // 809 The Packet Identifier field is only present in PUBLISH Packets where the QoS level is 1 or 2.

	tVariableHeaderPUBLISH() {}

	static std::expected<tVariableHeaderPUBLISH, tError> Parse(const hidden::tFixedHeader& fixedHeader, tSpan& data);

	std::size_t GetSize() const { return TopicName.GetSize() + (PacketId.has_value() ? tUInt16::GetSize() : 0); }

	std::string ToString() const;

	std::vector<std::uint8_t> ToVector() const;

	bool operator==(const tVariableHeaderPUBLISH& val) const;
};

struct tPayloadPUBLISH
{
	std::vector<std::uint8_t> Data; // The content and format of the data is application specific.

	static std::expected<tPayloadPUBLISH, tError> Parse(const tVariableHeaderPUBLISH& variableHeader, tSpan& data);

	std::string ToString() const { return std::string("Size: ") + std::to_string(Data.size()); }

	std::vector<std::uint8_t> ToVector() const { return Data; }

	bool operator==(const tPayloadPUBLISH& value) const = default;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PUBACK, PUBREC, PUBREL, PUBCOMP

struct tVariableHeaderPUBACK
{
	tUInt16 PacketId;

	tVariableHeaderPUBACK() = default;

	static std::expected<tVariableHeaderPUBACK, tError> Parse(const hidden::tFixedHeader& fixedHeader, tSpan& data);

	static std::size_t GetSize() { return tUInt16::GetSize(); }

	std::string ToString() const;

	std::vector<std::uint8_t> ToVector() const { return PacketId.ToVector(); }

	bool operator==(const tVariableHeaderPUBACK& val) const = default;
};

using tPayloadPUBACK = tPayloadEmpty<tVariableHeaderPUBACK>;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SUBSCRIBE

using tVariableHeaderSUBSCRIBE = tVariableHeaderPUBACK;

struct tPayloadSUBSCRIBE
{
	struct tTopicFilter
	{
		tString TopicFilter;
		tQoS QoS = tQoS::AtMostOnceDelivery;

		static std::expected<tTopicFilter, tError> Parse(tSpan& data);

		std::string ToString() const;

		std::vector<std::uint8_t> ToVector() const;

		std::size_t GetSize() const { return TopicFilter.GetSize() + sizeof(QoS); }

		bool operator==(const tTopicFilter& val) const = default;
	};

	std::vector<tTopicFilter> TopicFilters;

	static std::expected<tPayloadSUBSCRIBE, tError> Parse(const tVariableHeaderSUBSCRIBE& variableHeader, tSpan& data);

	std::size_t GetSize() const;

	std::string ToString() const;

	std::vector<std::uint8_t> ToVector() const;

	bool operator==(const tPayloadSUBSCRIBE& value) const = default;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SUBACK

using tVariableHeaderSUBACK = tVariableHeaderPUBACK;

struct tPayloadSUBACK
{
	tSubscribeReturnCode SubscribeReturnCode;

	static std::expected<tPayloadSUBACK, tError> Parse(const tVariableHeaderSUBACK& variableHeader, tSpan& data);

	static std::size_t GetSize() { return sizeof(tSubscribeReturnCode); }

	std::string ToString() const;

	std::vector<std::uint8_t> ToVector() const;

	bool operator==(const tPayloadSUBACK& value) const = default;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UNSUBSCRIBE

using tVariableHeaderUNSUBSCRIBE = tVariableHeaderPUBACK;

struct tPayloadUNSUBSCRIBE
{
	std::vector<tString> TopicFilters;

	static std::expected<tPayloadUNSUBSCRIBE, tError> Parse(const tVariableHeaderUNSUBSCRIBE& variableHeader, tSpan& data);

	std::size_t GetSize() const;

	std::string ToString() const;

	std::vector<std::uint8_t> ToVector() const;

	bool operator==(const tPayloadUNSUBSCRIBE& value) const = default;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UNSUBACK

using tVariableHeaderUNSUBACK = tVariableHeaderPUBACK;

using tPayloadUNSUBACK = tPayloadEmpty<tVariableHeaderUNSUBACK>;

} // hidden

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class tPacketCONNECT : public hidden::tPacket<hidden::tVariableHeaderCONNECT, hidden::tPayloadCONNECT>
{
public:
	tPacketCONNECT() :tPacket(GetFixedHeader()) {}
	tPacketCONNECT(bool cleanSession, std::uint16_t keepAlive, const std::string& clientId, tQoS willQos, bool willRetain, const std::string& willTopic, const std::string& willMessage, const std::string& userName, const std::string& password);
	tPacketCONNECT(bool cleanSession, std::uint16_t keepAlive, const std::string& clientId, tQoS willQos, bool willRetain, const std::string& willTopic, const std::string& willMessage)
		:tPacketCONNECT(cleanSession, keepAlive, clientId, willQos, willRetain, willTopic, willMessage, "", "")	{}
	tPacketCONNECT(bool cleanSession, std::uint16_t keepAlive, const std::string& clientId)
		:tPacketCONNECT(cleanSession, keepAlive, clientId, tQoS::AtMostOnceDelivery, false, "", "", "", "") {}
	tPacketCONNECT(bool cleanSession, std::uint16_t keepAlive, const std::string& clientId, const std::string& userName, const std::string& password)
		:tPacketCONNECT(cleanSession, keepAlive, clientId, tQoS::AtMostOnceDelivery, false, "", "", userName, password) {}

private:
	void SetClientId(std::string value);
	void SetWill(tQoS qos, bool retain, const std::string& topic, const std::string& message);
	void SetUser(const std::string& name, const std::string& password);

	static hidden::tFixedHeader GetFixedHeader() { return hidden::MakeFixedHeader(tControlPacketType::CONNECT); }
};

class tPacketCONNACK : public hidden::tPacket<hidden::tVariableHeaderCONNACK, hidden::tPayloadCONNACK>
{
public:
	tPacketCONNACK() :tPacket(GetFixedHeader()) {}
	tPacketCONNACK(bool sessionPresent, tConnectReturnCode connectRetCode)
		:tPacket(GetFixedHeader())
	{
		m_VariableHeader = hidden::tVariableHeaderCONNACK{};
		m_VariableHeader->ConnectAcknowledgeFlags.Field.SessionPresent = sessionPresent;
		m_VariableHeader->ConnectReturnCode = connectRetCode;
	}

private:
	static hidden::tFixedHeader GetFixedHeader() { return hidden::MakeFixedHeader(tControlPacketType::CONNACK); }
};

class tPacketPUBLISH : public hidden::tPacket<hidden::tVariableHeaderPUBLISH, hidden::tPayloadPUBLISH>
{
public:
	tPacketPUBLISH() = delete;
	tPacketPUBLISH(bool dup, bool retain, const std::string& topicName, tQoS qos, tUInt16 packetId);
	tPacketPUBLISH(bool dup, bool retain, const std::string& topicName, tQoS qos, tUInt16 packetId, const std::vector<std::uint8_t>& payloadData);
	tPacketPUBLISH(bool dup, bool retain, const std::string& topicName);
	tPacketPUBLISH(bool dup, bool retain, const std::string& topicName, const std::vector<std::uint8_t>& payloadData);

	// 809 The Packet Identifier field is only present in PUBLISH Packets where the QoS level is 1 or 2.
	static bool IsPacketIdPresent(std::uint8_t flags);

private:
	static hidden::tFixedHeader GetFixedHeader(bool dup, tQoS qos, bool retain);
};

// 848 A PUBACK Packet is the response to a PUBLISH Packet with QoS level 1. (AtLeastOnceDelivery)
class tPacketPUBACK : public hidden::tPacket<hidden::tVariableHeaderPUBACK, hidden::tPayloadPUBACK>
{
public:
	tPacketPUBACK() = delete;
	explicit tPacketPUBACK(tUInt16 packetId)
		:tPacket(GetFixedHeader())
	{
		m_VariableHeader = hidden::tVariableHeaderPUBACK{};
		m_VariableHeader->PacketId = packetId;
	}

private:
	static hidden::tFixedHeader GetFixedHeader() { return hidden::MakeFixedHeader(tControlPacketType::PUBACK); }
};

// 863 A PUBREC Packet is the response to a PUBLISH Packet with QoS 2. It is the second packet of the QoS
// 864 2 protocol exchange.
class tPacketPUBREC : public hidden::tPacket<hidden::tVariableHeaderPUBACK, hidden::tPayloadPUBACK>
{
public:
	tPacketPUBREC() = delete;
	explicit tPacketPUBREC(tUInt16 packetId)
		:tPacket(GetFixedHeader())
	{
		m_VariableHeader = hidden::tVariableHeaderPUBACK{};
		m_VariableHeader->PacketId = packetId;
	}

private:
	static hidden::tFixedHeader GetFixedHeader() { return hidden::MakeFixedHeader(tControlPacketType::PUBREC); }
};

// 879 A PUBREL Packet is the response to a PUBREC Packet.It is the third packet of the QoS 2 protocol
// 880 exchange.
class tPacketPUBREL : public hidden::tPacket<hidden::tVariableHeaderPUBACK, hidden::tPayloadPUBACK>
{
public:
	tPacketPUBREL() = delete;
	explicit tPacketPUBREL(tUInt16 packetId)
		:tPacket(GetFixedHeader())
	{
		m_VariableHeader = hidden::tVariableHeaderPUBACK{};
		m_VariableHeader->PacketId = packetId;
	}

private:
	static hidden::tFixedHeader GetFixedHeader() { return hidden::MakeFixedHeader(tControlPacketType::PUBREL); }
};

// 901 The PUBCOMP Packet is the response to a PUBREL Packet.It is the fourth and final packet of the QoS
// 902 2 protocol exchange.
class tPacketPUBCOMP : public hidden::tPacket<hidden::tVariableHeaderPUBACK, hidden::tPayloadPUBACK>
{
public:
	tPacketPUBCOMP() = delete;
	explicit tPacketPUBCOMP(tUInt16 packetId)
		:tPacket(GetFixedHeader())
	{
		m_VariableHeader = hidden::tVariableHeaderPUBACK{};
		m_VariableHeader->PacketId = packetId;
	}

private:
	static hidden::tFixedHeader GetFixedHeader() { return hidden::MakeFixedHeader(tControlPacketType::PUBCOMP); }
};

class tPacketSUBSCRIBE : public hidden::tPacket<hidden::tVariableHeaderSUBSCRIBE, hidden::tPayloadSUBSCRIBE>
{
public:
	tPacketSUBSCRIBE() = delete;
	tPacketSUBSCRIBE(tUInt16 packetId, const tString& topicFilter, tQoS qos)
		:tPacket(GetFixedHeader())
	{
		m_VariableHeader = hidden::tVariableHeaderSUBSCRIBE{};
		m_VariableHeader->PacketId = packetId;

		m_Payload = hidden::tPayloadSUBSCRIBE{};
		m_Payload->TopicFilters.emplace_back(topicFilter, qos);
	}

	void AddTopicFilter(const tString& topicFilter, tQoS qos)
	{
		m_Payload->TopicFilters.emplace_back(topicFilter, qos);
	}

private:
	static hidden::tFixedHeader GetFixedHeader() { return hidden::MakeFixedHeader(tControlPacketType::SUBSCRIBE); }
};

class tPacketSUBACK : public hidden::tPacket<hidden::tVariableHeaderSUBACK, hidden::tPayloadSUBACK>
{
public:
	tPacketSUBACK() = delete;
	explicit tPacketSUBACK(tUInt16 packetId, tSubscribeReturnCode subscribeReturnCode)
		:tPacket(GetFixedHeader())
	{
		m_VariableHeader = hidden::tVariableHeaderSUBACK{};
		m_VariableHeader->PacketId = packetId;

		m_Payload = hidden::tPayloadSUBACK{};
		m_Payload->SubscribeReturnCode = subscribeReturnCode;
	}

private:
	static hidden::tFixedHeader GetFixedHeader() { return hidden::MakeFixedHeader(tControlPacketType::SUBACK); }
};

class tPacketUNSUBSCRIBE : public hidden::tPacket<hidden::tVariableHeaderUNSUBSCRIBE, hidden::tPayloadUNSUBSCRIBE>
{
public:
	tPacketUNSUBSCRIBE() = delete;
	tPacketUNSUBSCRIBE(tUInt16 packetId, const tString& topicFilter)
		:tPacket(GetFixedHeader())
	{
		m_VariableHeader = hidden::tVariableHeaderUNSUBSCRIBE{};
		m_VariableHeader->PacketId = packetId;

		m_Payload = hidden::tPayloadUNSUBSCRIBE{};
		m_Payload->TopicFilters.push_back(topicFilter);
	}

	void AddTopicFilter(const tString& topicFilter)
	{
		m_Payload->TopicFilters.push_back(topicFilter);
	}

private:
	static hidden::tFixedHeader GetFixedHeader() { return hidden::MakeFixedHeader(tControlPacketType::UNSUBSCRIBE); }
};

class tPacketUNSUBACK : public hidden::tPacket<hidden::tVariableHeaderUNSUBACK, hidden::tPayloadUNSUBACK>
{
public:
	tPacketUNSUBACK() = delete;
	explicit tPacketUNSUBACK(tUInt16 packetId)
		:tPacket(GetFixedHeader())
	{
		m_VariableHeader = hidden::tVariableHeaderSUBACK{};
		m_VariableHeader->PacketId = packetId;
	}

private:
	static hidden::tFixedHeader GetFixedHeader() { return hidden::MakeFixedHeader(tControlPacketType::UNSUBACK); }
};

class tPacketPINGREQ : public hidden::tPacket<hidden::tVariableHeaderEmpty, hidden::tPayloadEmpty<hidden::tVariableHeaderEmpty>>
{
public:
	tPacketPINGREQ() :tPacket(GetFixedHeader()) {}

private:
	static hidden::tFixedHeader GetFixedHeader() { return hidden::MakeFixedHeader(tControlPacketType::PINGREQ); }
};

class tPacketPINGRESP : public hidden::tPacket<hidden::tVariableHeaderEmpty, hidden::tPayloadEmpty<hidden::tVariableHeaderEmpty>>
{
public:
	tPacketPINGRESP() :tPacket(GetFixedHeader()) {}

private:
	static hidden::tFixedHeader GetFixedHeader() { return hidden::MakeFixedHeader(tControlPacketType::PINGRESP); }
};

class tPacketDISCONNECT : public hidden::tPacket<hidden::tVariableHeaderEmpty, hidden::tPayloadEmpty<hidden::tVariableHeaderEmpty>>
{
public:
	tPacketDISCONNECT() :tPacket(GetFixedHeader()) {}

private:
	static hidden::tFixedHeader GetFixedHeader() { return hidden::MakeFixedHeader(tControlPacketType::DISCONNECT); }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::expected<tControlPacketType, tError> TestPacket(tSpan& data);
std::expected<tControlPacketType, tError> TestPacket(const std::vector<std::uint8_t>& data);

}
}
