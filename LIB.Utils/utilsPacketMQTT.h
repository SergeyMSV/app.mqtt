///////////////////////////////////////////////////////////////////////////////////////////////////
// utilsPacketMQTT
// 2024-11-22
// C++23
// 
// Specification: mqtt-v3.1.1-os.pdf (MQTT Version 3.1.1 OASIS Standard 29 October 2014)
///////////////////////////////////////////////////////////////////////////////////////////////////

// Find out:
// 1. packet MQTT can be the same for both MQTT-3.1.1 and MQTT-5.0
//    or compatible part can be defined separately

#include <expected> // C++ 23
#include <optional>
//#include <queue>
#include <span> // C++ 20
#include <string>
//#include <utility>
#include <vector>

#include <cstdint>

//#include <iostream> // TEST

namespace utils
{
namespace packet_MQTT
{
// ... you cannot use a string that would encode to more than 65535 bytes.
// Unless stated otherwise all UTF-8 encoded strings can have any length in the range 0 to 65535 bytes.

// The character data in a UTF-8 encoded string MUST be well-formed UTF-8 as defined by the Unicode 190 specification [Unicode] and restated in RFC 3629 [RFC3629].
// In particular this data MUST NOT include 191 encodings of code points between U+D800 and U+DFFF.
// If a Server or Client receives a Control Packet 192 containing ill-formed UTF-8 it MUST close the Network Connection [MQTT-1.5.3-1].

// A UTF-8 encoded string MUST NOT include an encoding of the null character U + 0000. If a receiver 195 (Server or Client) receives a Control Packet containing U + 0000 it MUST close the Network 196 Connection[MQTT - 1.5.3 - 2].

// A UTF-8 encoded sequence 0xEF 0xBB 0xBF is always to be interpreted to mean U+FEFF ("ZERO 207 WIDTH NO-BREAK SPACE") wherever it appears in a string and MUST NOT be skipped over or stripped 208 off by a packet receiver [MQTT-1.5.3-3].

enum class tControlPacketType
{
	//Reserved_1,		// Forbidden
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
	//Reserved_2		// Forbidden
};

enum class tError
{
	None,
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
	//Format,
	ProtocolName,
	ProtocolLevel,
};

enum class tQoS : std::uint8_t // CONNECT (WillQoS), PUBLISH
{
	AtMostOnceDelivery,
	AtLeastOnceDelivery,
	ExactlyOnceDelivery,
	Reserved_MustNotBeUsed
};

enum class tConnectReturnCode : std::uint8_t // CONNECT
{
	ConnectionAccepted,
	ConnectionRefused_UnacceptableProtocolVersion, // The Server does not support the level of the MQTT protocol requested by the Client.
	ConnectionRefused_IdentifierRejected, // The Client identifier is correct UTF-8 but not allowed by the Server.
	ConnectionRefused_ServerUnavailable, // The Network Connection has been made but the MQTT service is unavailable.
	ConnectionRefused_BadUserNameOrPassword, // The data in the user name or password is malformed.
	ConnectionRefused_NotAuthorized, // The Client is not authorized to connect.
	Reserved // 6-255 Reserved for future use.
};

class tSpan : public std::span<const std::uint8_t>
{
public:
	tSpan(const std::vector<std::uint8_t>& data) :std::span<const std::uint8_t>(data) {}
	tSpan(const std::vector<std::uint8_t>& data, std::size_t size) :std::span<const std::uint8_t>(data.begin(), std::min(size, data.size())) {}

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
	tString(const std::string& value) : std::string(value) {} // not explicit
	tString(std::string&& value) : std::string(value) {} // not explicit

	std::size_t GetSize() const { return size() + GetSizeMin(); }
	static constexpr std::size_t GetSizeMin() { return tUInt16::GetSize(); }

	static std::expected<tString, tError> Parse(tSpan& data);

	std::vector<std::uint8_t> ToVector() const;
};

namespace hidden
{

// MQTT 3.1.1
constexpr char DefaultProtocolName[] = "MQTT"; // The string, its offset and length will not be changed by future versions of the MQTT specification.
constexpr std::uint8_t DefaultProtocolLevel = 4; // The value of the Protocol Level field for the version 3.1.1 of the protocol is 4 (0x04).

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

	bool operator==(const tFixedHeader& val) const { return Value == val.Value; }
};

constexpr tFixedHeader MakeFixedHeader(tControlPacketType type, std::uint8_t flags = 0)
{
	tFixedHeader Header{};
	Header.Field.ControlPacketType = static_cast<std::uint8_t>(type);
	Header.Field.Flags = flags;
	return Header;
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
		
		switch (ControlPacketType)
		{
		case tControlPacketType::PINGREQ:
		case tControlPacketType::PINGRESP:
		case tControlPacketType::DISCONNECT:
			return Pack;
		}

		auto VarHeadExp = VH::Parse(Pack.m_FixedHeader, data);
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
			if (data.empty())
				return Pack;
			break;
		}

		if (ControlPacketType == tControlPacketType::CONNECT ||
			ControlPacketType == tControlPacketType::PUBLISH && !data.empty() ||
			ControlPacketType == tControlPacketType::SUBSCRIBE ||
			ControlPacketType == tControlPacketType::SUBACK ||
			ControlPacketType == tControlPacketType::UNSUBSCRIBE)
		{
			auto PayloadExp = PL::Parse(*Pack.m_VariableHeader, data);
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

	std::string ToString(bool extended = false) const { return m_VariableHeader->ToString(extended); }

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

	std::vector<std::uint8_t> ToVector() const { return {}; }

	bool operator==(const tVariableHeaderEmpty& value) const = default;
};

template <class VH>
struct tPayloadEmpty
{
	static std::expected<tPayloadEmpty, tError> Parse(const VH& variableHeader, tSpan& data) { return tPayloadEmpty(); }

	std::vector<std::uint8_t> ToVector() const { return {}; }

	bool operator==(const tPayloadEmpty& value) const = default;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CONNECT

struct tVariableHeaderCONNECT
{
	tString ProtocolName; // The string, its offset and length will not be changed by future versions of the MQTT specification.
	std::uint8_t ProtocolLevel; // The value of the Protocol Level field for the version 3.1.1 of the protocol is 4 (0x04).

	union tConnectFlags
	{
		struct
		{
			std::uint8_t Reserved : 1;
			std::uint8_t CleanSession : 1;
			std::uint8_t WillFlag : 1;
			std::uint8_t WillQoS : 2; // [TBD] find out how it works
			std::uint8_t WillRetain : 1;
			std::uint8_t PasswordFlag : 1;
			std::uint8_t UserNameFlag : 1;
		}Field;
		std::uint8_t Value = 0;
	}ConnectFlags;

	// 529 The Keep Alive is a time interval measured in seconds.
	// 
	// 538 If the Keep Alive value is non-zero and the Server does not receive a Control Packet from the Client
	// 539 within one and a half times the Keep Alive time period, it MUST disconnect the Network Connection to the
	// 540 Client as if the network had failed[MQTT-3.1.2-24].
	// 
	// 551 ... The maximum value is 18 hours 12 minutes and 15 seconds.
	tUInt16 KeepAlive = 0;

	tVariableHeaderCONNECT() :ProtocolName(hidden::DefaultProtocolName), ProtocolLevel(hidden::DefaultProtocolLevel) {}

	static std::expected<tVariableHeaderCONNECT, tError> Parse(const hidden::tFixedHeader& fixedHeader, tSpan& data);

	std::size_t GetSize() const { return ProtocolName.GetSize() + 4; }

	std::vector<std::uint8_t> ToVector() const;

	bool operator==(const tVariableHeaderCONNECT& val) const;
};

struct tPayloadCONNECT // The payload of the CONNECT Packet contains one or more length-prefixed fields, whose presence is determined by the flags in the variable header.
{
	// The Client Identifier (ClientId) identifies the Client to the Server. 
	// The Server MUST allow ClientIds which are between 1 and 23 UTF - 8 encoded bytes in length, and that contain only the characters "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ".
	tString ClientId;

	// These fields, if present, MUST appear in the order Client Identifier, Will Topic, Will Message, User Name, Password
	std::optional<tString> WillTopic;
	std::optional<tString> WillMessage;

	std::optional<tString> UserName;
	std::optional<tString> Password;

	static std::expected<tPayloadCONNECT, tError> Parse(const tVariableHeaderCONNECT& variableHeader, tSpan& data);

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
	tConnectReturnCode ConnectReturnCode = tConnectReturnCode::Reserved;

	tVariableHeaderCONNACK() = default;

	static std::expected<tVariableHeaderCONNACK, tError> Parse(const hidden::tFixedHeader& fixedHeader, tSpan& data);

	static std::size_t GetSize() { return 2; }

	std::string ToString(bool extended = false) const;

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
	std::optional<tUInt16> PacketId; // The Packet Identifier field is only present in PUBLISH Packets where the QoS level is 1 or 2.

	tVariableHeaderPUBLISH() {}

	static std::expected<tVariableHeaderPUBLISH, tError> Parse(const hidden::tFixedHeader& fixedHeader, tSpan& data);

	std::size_t GetSize() const { return TopicName.GetSize() + 2; }

	std::vector<std::uint8_t> ToVector() const;

	bool operator==(const tVariableHeaderPUBLISH& val) const;
};

struct tPayloadPUBLISH
{
	std::vector<std::uint8_t> Data; // The content and format of the data is application specific.

	static std::expected<tPayloadPUBLISH, tError> Parse(const tVariableHeaderPUBLISH& variableHeader, tSpan& data);

	std::vector<std::uint8_t> ToVector() const { return Data; }

	bool operator==(const tPayloadPUBLISH& value) const = default;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PUBACK

struct tVariableHeaderPUBACK
{
	tUInt16 PacketId; // This contains the Packet Identifier from the PUBLISH Packet that is being acknowledged.

	tVariableHeaderPUBACK() = default;

	static std::expected<tVariableHeaderPUBACK, tError> Parse(const hidden::tFixedHeader& fixedHeader, tSpan& data);

	static std::size_t GetSize() { return 2; } // For the PUBACK Packet this has the value 2.

	std::vector<std::uint8_t> ToVector() const { return PacketId.ToVector(); }

	bool operator==(const tVariableHeaderPUBACK& val) const = default;
};

using tPayloadPUBACK = tPayloadEmpty<tVariableHeaderPUBACK>;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PUB...

// [TBD] the other packets

} // hidden

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class tPacketCONNECT : public hidden::tPacket<hidden::tVariableHeaderCONNECT, hidden::tPayloadCONNECT>
{
public:
	tPacketCONNECT() :tPacket(GetFixedHeader()) {}
	tPacketCONNECT(bool cleanSession, std::uint16_t keepAlive, const std::string& clientId, const std::string& willTopic, const std::string& willMessage, const std::string& userName, const std::string& password);
	tPacketCONNECT(bool cleanSession, std::uint16_t keepAlive, const std::string& clientId, const std::string& willTopic, const std::string& willMessage)
		:tPacketCONNECT(cleanSession, keepAlive, clientId, willTopic, willMessage, "", "")	{}

private:
	void SetClientId(std::string value);
	void SetWill(const std::string& topic, const std::string& message);
	void SetUser(const std::string& name, const std::string& password);

	static hidden::tFixedHeader GetFixedHeader() { return hidden::MakeFixedHeader(tControlPacketType::CONNECT); }
};

class tPacketCONNACK : public hidden::tPacket<hidden::tVariableHeaderCONNACK, hidden::tPayloadCONNACK>
{
public:
	tPacketCONNACK() :tPacket(GetFixedHeader()) {}
	tPacketCONNACK(bool sessionPresent, tConnectReturnCode connRetCode)
		:tPacket(GetFixedHeader())
	{
		m_VariableHeader = hidden::tVariableHeaderCONNACK{};
		m_VariableHeader->ConnectAcknowledgeFlags.Field.SessionPresent = sessionPresent;
		m_VariableHeader->ConnectReturnCode = connRetCode;
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

	// The Packet Identifier field is only present in PUBLISH Packets where the QoS level is 1 or 2.
	static bool IsPacketIdPresent(std::uint8_t flags);

private:
	static hidden::tFixedHeader GetFixedHeader(bool dup, tQoS qos, bool retain);
};

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


// [TBD] the other packets


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
