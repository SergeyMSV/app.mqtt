///////////////////////////////////////////////////////////////////////////////////////////////////
// utilsPacketMQTT
// 2024-11-22
// C++20
// 
// Specification: mqtt-v3.1.1.pdf (MQTT Version 3.1.1 Plus Errata 01; OASIS Standard Incorporating Approved Errata 01; 10 December 2015)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <libConfig.h>

#include "utilsStd.h"

#include <optional>
#include <span> // C++ 20
#include <string>
#include <vector>

#include <cstdint>

namespace utils
{
namespace packet
{
namespace mqtt
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

enum class tQoS : std::uint8_t
{
	AtMostOnceDelivery, // (Fire and Forget)
	AtLeastOnceDelivery, // (Acknowledged deliver)
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

template<typename T> std::string ToString(T val);

class tSpan : public std::span<const std::uint8_t>
{
public:
	tSpan(const std::vector<std::uint8_t>& data) :std::span<const std::uint8_t>(data) {}
	tSpan(const std::vector<std::uint8_t>& data, std::size_t size) :std::span<const std::uint8_t>(data.begin(), std::min(size, data.size())) {}
	tSpan(tSpan& data, std::size_t size) :std::span<const std::uint8_t>(data.begin(), std::min(size, data.size())) {}

	tSpan& operator=(const tSpan&) = default;

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
	tUInt16(std::uint16_t val) :Value(val) {} // not explicit

	static constexpr std::size_t GetSize() { return 2; }

	static std::optional<tUInt16> Parse(tSpan& data);

	std::vector<std::uint8_t> ToVector() const;

	tUInt16& operator=(std::uint16_t val);
	bool operator==(const tUInt16& val) const { return Value == val.Value; }
};
#pragma pack(pop)

class tString : public std::string
{
public:
	tString() = default;
	tString(const char* val) :std::string(val) {} // not explicit
	tString(const std::string& val) :std::string(val) {} // not explicit
	tString(std::string&& val) :std::string(val) {} // not explicit

	std::size_t GetSize() const { return size() + GetSizeMin(); }
	static constexpr std::size_t GetSizeMin() { return tUInt16::GetSize(); }

	static std::optional <tString> Parse(tSpan& data);

	std::vector<std::uint8_t> ToVector() const;
};

class tPacket
{
public:
	virtual ~tPacket() {}

	virtual std::string ToString() const = 0;

	virtual std::vector<std::uint8_t> ToVector() const = 0;
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
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T>
bool operator==(const std::optional<T>& val1, const std::optional<T>& val2)
{
	return val1.has_value() != val2.has_value() ? false : val1.has_value() == false ? true : *val1 == *val2;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

union tFixedHeader
{
	struct
	{
		std::uint8_t Flags : 4;
		std::uint8_t ControlPacketType : 4;
	}Field;
	std::uint8_t Value = 0;

	tFixedHeader() = default;
	tFixedHeader(std::uint8_t val) :Value(val) {} // not explicit

	static std::optional<std::pair<tFixedHeader, std::size_t>> Parse(tSpan& data);

	tControlPacketType GetControlPacketType() const { return static_cast<tControlPacketType>(Field.ControlPacketType); }

	std::string ToString(bool align = false) const;

	std::vector<std::uint8_t> ToVector(std::size_t dataSize) const;

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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
	static std::optional<std::uint32_t> Parse(tSpan& data);
	static std::vector<std::uint8_t> ToVector(std::uint32_t val);
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename TCont>
class tPacketBase : public tPacket
{
protected:
	TCont m_Content;

private:
	tPacketBase() = default;

protected:
	explicit tPacketBase(const TCont& content) :m_Content(content) {}

public:
	tPacketBase(const tPacketBase&) = default;
	tPacketBase(tPacketBase&& val) noexcept
	{
		m_Content = std::move(val.m_Content);
	}

	tPacketBase& operator=(const tPacketBase&) = default;
	tPacketBase& operator=(tPacketBase&& val)
	{
		if (this == &val)
			return *this;
		m_Content = std::move(val.m_Content);
		return *this;
	}

	TCont::fixed_header_type GetFixedHeader() const { return m_Content.FixedHeader; }
	TCont::variable_header_type GetVariableHeader() const { return m_Content.VariableHeader; }
	TCont::payload_type GetPayload() const { return m_Content.Payload; }

	static std::optional<tPacketBase> Parse(tSpan& data)
	{
		if (data.empty())
			return {};
		std::optional<TCont> Cont = TCont::Parse(data);
		if (!Cont.has_value())
			return {};
		tPacketBase Pack{};
		Pack.m_Content = std::move(*Cont);
		return Pack;
	}

	static std::optional<tPacketBase> Parse(const std::vector<std::uint8_t>& data)
	{
		tSpan DataSpan(data);
		return Parse(DataSpan);
	}

	std::string ToString() const { return m_Content.ToString(); }

	std::vector<std::uint8_t> ToVector() const { return m_Content.ToVector(); }

	bool operator==(const tPacketBase& val) const
	{
		return m_Content == val.m_Content;
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// EMPTY

template<tControlPacketType ControlPacketType>
struct tContentEMPTY
{
	tFixedHeader FixedHeader{};

	typedef typename tFixedHeader fixed_header_type;
	typedef typename void variable_header_type;
	typedef typename void payload_type;

	tContentEMPTY() :FixedHeader(GetFixedHeader()) {}

	static std::optional<tContentEMPTY> Parse(tSpan& data)
	{
		std::optional<std::pair<tFixedHeader, std::size_t>> FixedHeaderOpt = tFixedHeader::Parse(data);
		if (!FixedHeaderOpt.has_value())
			return {};
		tContentEMPTY Content{};
		Content.FixedHeader = FixedHeaderOpt->first;
		return Content;
	}

	std::string ToString() const { return FixedHeader.ToString(true); }

	std::vector<std::uint8_t> ToVector() const { return FixedHeader.ToVector(0); }

	bool operator==(const tContentEMPTY& val) const = default;

private:
	static tFixedHeader GetFixedHeader() { return MakeFixedHeader(ControlPacketType); }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CONNECT

struct tContentCONNECT
{
	tFixedHeader FixedHeader{};
	struct tVariableHeader
	{
		tString ProtocolName;
		std::uint8_t ProtocolLevel = 0;

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

		tVariableHeader() = default;
		tVariableHeader(const tVariableHeader&) = default;
		tVariableHeader(tVariableHeader&& val) noexcept;
		tVariableHeader& operator=(const tVariableHeader&) = default;
		tVariableHeader& operator=(tVariableHeader&& val) noexcept;

		bool operator==(const tVariableHeader& val) const;
	}VariableHeader;

	struct tPayload
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

		tPayload() = default;
		tPayload(const tPayload&) = default;
		tPayload(tPayload&& val) noexcept;
		tPayload& operator=(const tPayload&) = default;
		tPayload& operator=(tPayload&& val) noexcept;

		bool operator==(const tPayload& val) const;
	}Payload;

	typedef typename tFixedHeader fixed_header_type;
	typedef typename tVariableHeader variable_header_type;
	typedef typename tPayload payload_type;

	tContentCONNECT() = default;
	tContentCONNECT(bool cleanSession, std::uint16_t keepAlive, const std::string& clientId, tQoS willQos, bool willRetain, const std::string& willTopic, const std::string& willMessage, const std::string& userName, const std::string& password);
	tContentCONNECT(bool cleanSession, std::uint16_t keepAlive, const std::string& clientId, tQoS willQos, bool willRetain, const std::string& willTopic, const std::string& willMessage);
	tContentCONNECT(bool cleanSession, std::uint16_t keepAlive, const std::string& clientId);
	tContentCONNECT(bool cleanSession, std::uint16_t keepAlive, const std::string& clientId, const std::string& userName, const std::string& password);
	tContentCONNECT(const tContentCONNECT&) = default;
	tContentCONNECT(tContentCONNECT&& val) noexcept;

	static std::optional<tContentCONNECT> Parse(tSpan& data);

	void SetClientId(std::string val);
	void SetWill(tQoS qos, bool retain, const std::string& topic, const std::string& message);
	void SetUser(const std::string& name, const std::string& password);

	std::string ToString() const;

	std::vector<std::uint8_t> ToVector() const;

	tContentCONNECT& operator=(const tContentCONNECT& val) = default;
	tContentCONNECT& operator=(tContentCONNECT&& val) noexcept;

	bool operator==(const tContentCONNECT& val) const = default;

private:
	static tFixedHeader GetFixedHeader() { return MakeFixedHeader(tControlPacketType::CONNECT); }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CONNACK

struct tContentCONNACK
{
	tFixedHeader FixedHeader{};
	struct tVariableHeader
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

		bool operator==(const tVariableHeader& val) const
		{
			return ConnectAcknowledgeFlags.Value == val.ConnectAcknowledgeFlags.Value && ConnectReturnCode == val.ConnectReturnCode;
		}
	}VariableHeader;

	typedef typename tFixedHeader fixed_header_type;
	typedef typename tVariableHeader variable_header_type;
	typedef typename void payload_type;

	tContentCONNACK() = default;
	tContentCONNACK(bool sessionPresent, tConnectReturnCode connectRetCode);

	static std::optional<tContentCONNACK> Parse(tSpan& data);

	std::string ToString() const;

	std::vector<std::uint8_t> ToVector() const;

	bool operator==(const tContentCONNACK& val) const = default;

private:
	static tFixedHeader GetFixedHeader() { return MakeFixedHeader(tControlPacketType::CONNACK); }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLISH

struct tContentPUBLISH
{
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

	tFixedHeader FixedHeader{};
	struct tVariableHeader
	{
		tString TopicName;
		std::optional<tUInt16> PacketId; // 809 The Packet Identifier field is only present in PUBLISH Packets where the QoS level is 1 or 2.

		tVariableHeader() = default;
		tVariableHeader(const tVariableHeader&) = default;
		tVariableHeader(tVariableHeader&& val) noexcept;

		tVariableHeader& operator=(const tVariableHeader& val) = default;
		tVariableHeader& operator=(tVariableHeader&& val) noexcept;

		bool operator==(const tVariableHeader& val) const = default;
	}VariableHeader;
	std::vector<std::uint8_t> Payload; // The content and format of the data is application specific.

	typedef typename tFixedHeader fixed_header_type;
	typedef typename tVariableHeader variable_header_type;
	typedef typename std::vector<std::uint8_t> payload_type;

	tContentPUBLISH() = default;
	tContentPUBLISH(bool dup, bool retain, const std::string& topicName);
	tContentPUBLISH(bool dup, bool retain, const std::string& topicName, const std::vector<std::uint8_t>& payload);
	tContentPUBLISH(bool dup, bool retain, const std::string& topicName, tQoS qos, tUInt16 packetId);
	tContentPUBLISH(bool dup, bool retain, const std::string& topicName, tQoS qos, tUInt16 packetId, const std::vector<std::uint8_t>& payload);
	tContentPUBLISH(const tContentPUBLISH&) = default;
	tContentPUBLISH(tContentPUBLISH&& val) noexcept;

	static std::optional<tContentPUBLISH> Parse(tSpan& data);

	std::string ToString() const;

	std::vector<std::uint8_t> ToVector() const;

	tContentPUBLISH& operator=(const tContentPUBLISH& val) = default;
	tContentPUBLISH& operator=(tContentPUBLISH&& val) noexcept;

	bool operator==(const tContentPUBLISH& val) const = default;

private:
	// 809 The Packet Identifier field is only present in PUBLISH Packets where the QoS level is 1 or 2.
	static bool IsPacketIdPresent(std::uint8_t flags);

	tFixedHeader GetFixedHeader(bool dup, tQoS qos, bool retain) const; // [TBD] get rid of it
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PUBACK, PUBREC, PUBREL, PUBCOMP

template<tControlPacketType ControlPacketType>
struct tContentPUB
{
	tFixedHeader FixedHeader{};
	struct tVariableHeader
	{
		tUInt16 PacketId;

		bool operator==(const tVariableHeader& val) const = default;
	}VariableHeader;

	typedef typename tFixedHeader fixed_header_type;
	typedef typename tVariableHeader variable_header_type;
	typedef typename void payload_type;

	tContentPUB() = default;
	explicit tContentPUB(tUInt16 packetId)
		:FixedHeader(GetFixedHeader())
	{
		VariableHeader.PacketId = packetId;
	}

	static std::optional<tContentPUB> Parse(tSpan& data)
	{
		std::optional<std::pair<tFixedHeader, std::size_t>> FixedHeaderOpt = tFixedHeader::Parse(data);
		if (!FixedHeaderOpt.has_value())
			return {};

		tContentPUB Content{};
		Content.FixedHeader = FixedHeaderOpt->first;

		std::optional<tUInt16> PacketIdOpt = tUInt16::Parse(data);
		if (!PacketIdOpt.has_value())
			return {};
		Content.VariableHeader.PacketId = *PacketIdOpt;

		return Content;
	}

	std::string ToString() const
	{
		std::string Str = FixedHeader.ToString(true);
		Str += " Packet ID: " + std::to_string(VariableHeader.PacketId.Value);
		return Str;
	}

	std::vector<std::uint8_t> ToVector() const
	{
		std23::vector<std::uint8_t> Data;
		Data.append_range(VariableHeader.PacketId.ToVector());
		Data.insert_range(Data.begin(), FixedHeader.ToVector(Data.size()));
		return Data;
	}

	bool operator==(const tContentPUB& val) const
	{
		return FixedHeader == val.FixedHeader && VariableHeader == val.VariableHeader;
	}

private:
	static tFixedHeader GetFixedHeader() { return MakeFixedHeader(ControlPacketType); }
};

using tContentPUBACK = tContentPUB<tControlPacketType::PUBACK>;
using tContentPUBREC = tContentPUB<tControlPacketType::PUBREC>;
using tContentPUBREL = tContentPUB<tControlPacketType::PUBREL>;
using tContentPUBCOMP = tContentPUB<tControlPacketType::PUBCOMP>;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SUBSCRIBE
/*
using tVariableHeaderSUBSCRIBE = tVariableHeaderPUBACK;

struct tPayloadSUBSCRIBE
{
	struct tTopicFilter
	{
		tString TopicFilter;
		tQoS QoS = tQoS::AtMostOnceDelivery;

		static std::optional<tTopicFilter> Parse(tSpan& data);

		std::string ToString() const;

		std::vector<std::uint8_t> ToVector() const;

		std::size_t GetSize() const { return TopicFilter.GetSize() + sizeof(QoS); }

		bool operator==(const tTopicFilter&) const = default;
	};

	std::vector<tTopicFilter> TopicFilters;

	static std::optional<tPayloadSUBSCRIBE> Parse(const tVariableHeaderSUBSCRIBE& variableHeader, tSpan& data);

	std::size_t GetSize() const;

	std::string ToString() const;

	std::vector<std::uint8_t> ToVector() const;

	bool operator==(const tPayloadSUBSCRIBE&) const = default;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SUBACK

using tVariableHeaderSUBACK = tVariableHeaderPUBACK;

struct tPayloadSUBACK
{
	tSubscribeReturnCode SubscribeReturnCode;

	static std::optional<tPayloadSUBACK> Parse(const tVariableHeaderSUBACK& variableHeader, tSpan& data);

	static std::size_t GetSize() { return sizeof(tSubscribeReturnCode); }

	std::string ToString() const;

	std::vector<std::uint8_t> ToVector() const;

	bool operator==(const tPayloadSUBACK&) const = default;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UNSUBSCRIBE

using tVariableHeaderUNSUBSCRIBE = tVariableHeaderPUBACK;

struct tPayloadUNSUBSCRIBE
{
	std::vector<tString> TopicFilters;

	static std::optional<tPayloadUNSUBSCRIBE> Parse(const tVariableHeaderUNSUBSCRIBE& variableHeader, tSpan& data);

	std::size_t GetSize() const;

	std::string ToString() const;

	std::vector<std::uint8_t> ToVector() const;

	bool operator==(const tPayloadUNSUBSCRIBE&) const = default;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UNSUBACK

using tVariableHeaderUNSUBACK = tVariableHeaderPUBACK;

using tPayloadUNSUBACK = tPayloadEmpty<tVariableHeaderUNSUBACK>;
*/
} // hidden

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class tPacketNOACK
{
public:
	static std::optional<tPacketNOACK> Parse(tSpan& data) { return {}; }

	std::string ToString() const { return {}; }

	std::vector<std::uint8_t> ToVector() const { return {}; }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class tPacketCONNACK : public hidden::tPacketBase<hidden::tContentCONNACK>
{
public:
	tPacketCONNACK() = delete;
	tPacketCONNACK(bool sessionPresent, tConnectReturnCode connectRetCode)
		:tPacketBase(hidden::tContentCONNACK(sessionPresent, connectRetCode))
	{
	}
	tPacketCONNACK(const hidden::tPacketBase<hidden::tContentCONNACK>& val) :tPacketBase(val) {} // for std::future<..>
	tPacketCONNACK(hidden::tPacketBase<hidden::tContentCONNACK>&& val) :tPacketBase(std::move(val)) {} // for std::future<..>

	//static tControlPacketType GetControlPacketType() { return tControlPacketType::CONNACK; }
};

class tPacketCONNECT : public hidden::tPacketBase<hidden::tContentCONNECT>
{
public:
	typedef typename tPacketCONNACK response_type;

	tPacketCONNECT() = delete;
	tPacketCONNECT(bool cleanSession, std::uint16_t keepAlive, const std::string& clientId, tQoS willQos, bool willRetain, const std::string& willTopic, const std::string& willMessage, const std::string& userName, const std::string& password)
		:tPacketBase(hidden::tContentCONNECT(cleanSession, keepAlive, clientId, willQos, willRetain, willTopic, willMessage, userName, password))
	{
	}
	tPacketCONNECT(bool cleanSession, std::uint16_t keepAlive, const std::string& clientId, tQoS willQos, bool willRetain, const std::string& willTopic, const std::string& willMessage)
		:tPacketBase(hidden::tContentCONNECT(cleanSession, keepAlive, clientId, willQos, willRetain, willTopic, willMessage))
	{
	}
	tPacketCONNECT(bool cleanSession, std::uint16_t keepAlive, const std::string& clientId, const std::string& userName, const std::string& password)
		:tPacketBase(hidden::tContentCONNECT(cleanSession, keepAlive, clientId, userName, password))
	{
	}
	tPacketCONNECT(bool cleanSession, std::uint16_t keepAlive, const std::string& clientId)
		:tPacketBase(hidden::tContentCONNECT(cleanSession, keepAlive, clientId))
	{
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// 848 A PUBACK Packet is the response to a PUBLISH Packet with QoS level 1. (AtLeastOnceDelivery)
class tPacketPUBACK : public hidden::tPacketBase<hidden::tContentPUBACK>
{
public:
	tPacketPUBACK() = delete;
	explicit tPacketPUBACK(tUInt16 packetId)
		:tPacketBase(hidden::tContentPUBACK(packetId))
	{
	}
	tPacketPUBACK(const hidden::tPacketBase<hidden::tContentPUBACK>& val) :tPacketBase(val) {} // for std::future<..>, std::optional<..>
	tPacketPUBACK(hidden::tPacketBase<hidden::tContentPUBACK>&& val) :tPacketBase(std::move(val)) {} // for std::future<..>, std::optional<..>
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// 863 A PUBREC Packet is the response to a PUBLISH Packet with QoS 2. It is the second packet of the QoS
// 864 2 protocol exchange.
class tPacketPUBREC : public hidden::tPacketBase<hidden::tContentPUBREC>
{
public:
	tPacketPUBREC() = delete;
	explicit tPacketPUBREC(tUInt16 packetId)
		:tPacketBase(hidden::tContentPUBREC(packetId))
	{
	}
	tPacketPUBREC(const hidden::tPacketBase<hidden::tContentPUBREC>& val) :tPacketBase(val) {} // for std::future<..>, std::optional<..>
	tPacketPUBREC(hidden::tPacketBase<hidden::tContentPUBREC>&& val) :tPacketBase(std::move(val)) {} // for std::future<..>, std::optional<..>
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<tQoS qos>
class tPacketPUBLISH : public hidden::tPacketBase<hidden::tContentPUBLISH>
{
};

template<>
class tPacketPUBLISH<tQoS::AtMostOnceDelivery> : public hidden::tPacketBase<hidden::tContentPUBLISH>
{
public:
	typedef typename tPacketNOACK response_type;

	tPacketPUBLISH() = delete;
	tPacketPUBLISH(bool dup, bool retain, const std::string& topicName)
		:tPacketBase(hidden::tContentPUBLISH(dup, retain, topicName))
	{
	}
	tPacketPUBLISH(bool dup, bool retain, const std::string& topicName, const std::vector<std::uint8_t>& payload)
		:tPacketBase(hidden::tContentPUBLISH(dup, retain, topicName, payload))
	{
	}
};

template<>
class tPacketPUBLISH<tQoS::AtLeastOnceDelivery> : public hidden::tPacketBase<hidden::tContentPUBLISH>
{
public:
	typedef typename tPacketPUBACK response_type;

	tPacketPUBLISH() = delete;
	tPacketPUBLISH(bool dup, bool retain, const std::string& topicName, tUInt16 packetId)
		:tPacketBase(hidden::tContentPUBLISH(dup, retain, topicName, tQoS::AtLeastOnceDelivery, packetId))
	{
	}
	tPacketPUBLISH(bool dup, bool retain, const std::string& topicName, tUInt16 packetId, const std::vector<std::uint8_t>& payload)
		:tPacketBase(hidden::tContentPUBLISH(dup, retain, topicName, tQoS::AtLeastOnceDelivery, packetId, payload))
	{
	}
};

template<>
class tPacketPUBLISH<tQoS::ExactlyOnceDelivery> : public hidden::tPacketBase<hidden::tContentPUBLISH>
{
public:
	typedef typename tPacketPUBREC response_type;

	tPacketPUBLISH() = delete;
	tPacketPUBLISH(bool dup, bool retain, const std::string& topicName, tUInt16 packetId)
		:tPacketBase(hidden::tContentPUBLISH(dup, retain, topicName, tQoS::ExactlyOnceDelivery, packetId))
	{
	}
	tPacketPUBLISH(bool dup, bool retain, const std::string& topicName, tUInt16 packetId, const std::vector<std::uint8_t>& payload)
		:tPacketBase(hidden::tContentPUBLISH(dup, retain, topicName, tQoS::ExactlyOnceDelivery, packetId, payload))
	{
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// 901 The PUBCOMP Packet is the response to a PUBREL Packet.It is the fourth and final packet of the QoS
// 902 2 protocol exchange.
class tPacketPUBCOMP : public hidden::tPacketBase<hidden::tContentPUBCOMP>
{
public:
	tPacketPUBCOMP() = delete;
	explicit tPacketPUBCOMP(tUInt16 packetId)
		:tPacketBase(hidden::tContentPUBCOMP(packetId))
	{
	}
	tPacketPUBCOMP(const hidden::tPacketBase<hidden::tContentPUBCOMP>& val) :tPacketBase(val) {} // for std::future<..>, std::optional<..>
	tPacketPUBCOMP(hidden::tPacketBase<hidden::tContentPUBCOMP>&& val) :tPacketBase(std::move(val)) {} // for std::future<..>, std::optional<..>
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// 879 A PUBREL Packet is the response to a PUBREC Packet.It is the third packet of the QoS 2 protocol
// 880 exchange.
class tPacketPUBREL : public hidden::tPacketBase<hidden::tContentPUBREL>
{
public:
	typedef typename tPacketPUBCOMP response_type;

	tPacketPUBREL() = delete;
	explicit tPacketPUBREL(tUInt16 packetId)
		:tPacketBase(hidden::tContentPUBREL(packetId))
	{
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
class tPacketSUBSCRIBE : public hidden::tPacketBase<hidden::tVariableHeaderSUBSCRIBE, hidden::tPayloadSUBSCRIBE>
{
public:
	tPacketSUBSCRIBE() = delete;
	tPacketSUBSCRIBE(tUInt16 packetId, const tString& topicFilter, tQoS qos)
		:tPacketBase(GetFixedHeader())
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

class tPacketSUBACK : public hidden::tPacketBase<hidden::tVariableHeaderSUBACK, hidden::tPayloadSUBACK>
{
public:
	tPacketSUBACK() = delete;
	explicit tPacketSUBACK(tUInt16 packetId, tSubscribeReturnCode subscribeReturnCode)
		:tPacketBase(GetFixedHeader())
	{
		m_VariableHeader = hidden::tVariableHeaderSUBACK{};
		m_VariableHeader->PacketId = packetId;

		m_Payload = hidden::tPayloadSUBACK{};
		m_Payload->SubscribeReturnCode = subscribeReturnCode;
	}

private:
	static hidden::tFixedHeader GetFixedHeader() { return hidden::MakeFixedHeader(tControlPacketType::SUBACK); }
};

class tPacketUNSUBSCRIBE : public hidden::tPacketBase<hidden::tVariableHeaderUNSUBSCRIBE, hidden::tPayloadUNSUBSCRIBE>
{
public:
	tPacketUNSUBSCRIBE() = delete;
	tPacketUNSUBSCRIBE(tUInt16 packetId, const tString& topicFilter)
		:tPacketBase(GetFixedHeader())
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

class tPacketUNSUBACK : public hidden::tPacketBase<hidden::tVariableHeaderUNSUBACK, hidden::tPayloadUNSUBACK>
{
public:
	tPacketUNSUBACK() = delete;
	explicit tPacketUNSUBACK(tUInt16 packetId)
		:tPacketBase(GetFixedHeader())
	{
		m_VariableHeader = hidden::tVariableHeaderSUBACK{};
		m_VariableHeader->PacketId = packetId;
	}

private:
	static hidden::tFixedHeader GetFixedHeader() { return hidden::MakeFixedHeader(tControlPacketType::UNSUBACK); }
};
*/

class tPacketPINGRESP : public hidden::tPacketBase<hidden::tContentEMPTY<tControlPacketType::PINGRESP>>
{
public:
	tPacketPINGRESP() :tPacketBase(hidden::tContentEMPTY<tControlPacketType::PINGRESP>()) {}
	tPacketPINGRESP(const hidden::tPacketBase<hidden::tContentEMPTY<tControlPacketType::PINGRESP>>& val) :tPacketBase(val) {} // for std::future<..>, std::optional<..>
	tPacketPINGRESP(hidden::tPacketBase<hidden::tContentEMPTY<tControlPacketType::PINGRESP>>&& val) :tPacketBase(std::move(val)) {} // for std::future<..>, std::optional<..>
};

class tPacketPINGREQ : public hidden::tPacketBase<hidden::tContentEMPTY<tControlPacketType::PINGREQ>>
{
public:
	typedef typename tPacketPINGRESP response_type;

	tPacketPINGREQ() :tPacketBase(hidden::tContentEMPTY<tControlPacketType::PINGREQ>()) {}
};

class tPacketDISCONNECT : public hidden::tPacketBase<hidden::tContentEMPTY<tControlPacketType::DISCONNECT>>
{
public:
	typedef typename tPacketNOACK response_type;

	tPacketDISCONNECT() :tPacketBase(hidden::tContentEMPTY<tControlPacketType::DISCONNECT>()) {}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::optional<tControlPacketType> TestPacket(tSpan& data);
std::optional<tControlPacketType> TestPacket(const std::vector<std::uint8_t>& data);

}
}
}
