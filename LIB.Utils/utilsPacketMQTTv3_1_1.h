///////////////////////////////////////////////////////////////////////////////////////////////////
// utilsPacketMQTTv3_1_1
// 2024-11-22
// C++20
// 
// Specification: mqtt-v3.1.1.pdf (MQTT Version 3.1.1 Plus Errata 01; OASIS Standard Incorporating Approved Errata 01; 10 December 2015)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <libConfig.h>

#include "utilsStd.h"

#include <algorithm>
#include <optional>
#include <span> // C++ 20
#include <string>
#include <utility>
#include <vector>

#include <cstdint>

namespace utils
{
namespace packet
{
namespace mqtt_3_1_1
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
	_None = 0,
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
	ExactlyOnceDelivery, // (Assured Delivery)
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
	SuccessMaximumQoS_AtMostOnceDelivery, // = tQoS::AtMostOnceDelivery,
	SuccessMaximumQoS_AtLeastOnceDelivery, // = tQoS::AtLeastOnceDelivery,
	SuccessMaximumQoS_ExactlyOnceDelivery, // = tQoS::ExactlyOnceDelivery,
	Failure = 0x80,
};

enum class tSessionState
{
	New,
	Present,
};

enum class tSessionStateRequest
{
	Continue, // open a "present" session if possible (tSessionState::Present)
	Clean, // open a "new" session (tSessionState::New)
};

template<typename T> std::string ToString(T val);

class tSpan : public std::span<const std::uint8_t>
{
public:
	tSpan(const std::vector<std::uint8_t>& data) :std::span<const std::uint8_t>(data) {}
	tSpan(const std::vector<std::uint8_t>& data, std::size_t size) :std::span<const std::uint8_t>(data.begin(), std::min(size, data.size())) {}
	tSpan(std::vector<std::uint8_t>::const_iterator data_begin, std::size_t size) :std::span<const std::uint8_t>(data_begin, size) {}
	tSpan(tSpan& data, std::size_t size) :std::span<const std::uint8_t>(data.begin(), std::min(size, data.size())) {}

	tSpan& operator=(const tSpan&) = default;

	void Skip(std::size_t count)
	{
		if (count >= size())
			count = size();
		*static_cast<std::span<const std::uint8_t>*>(this) = last(size() - count);
	}

	void Shorten(std::size_t count)
	{
		if (count >= size())
			count = size();
		*static_cast<std::span<const std::uint8_t>*>(this) = first(count);
	}

	std::vector<std::uint8_t> ToVector() const { return std::vector<std::uint8_t>(begin(), end()); }
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
	virtual std::string ToStringControlPacketType() const = 0;

	virtual std::vector<std::uint8_t> ToVector() const = 0;
};

namespace hidden
{

namespace mqtt_main = mqtt_3_1_1;

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

class tFixedHeaderBase
{
protected:
	union tData
	{
		struct
		{
			std::uint8_t Flags : 4;
			std::uint8_t ControlPacketType : 4;
		}Field;
		struct
		{
			// Normally if a publisher publishes a message to a topic, and no one is subscribed to that topic the message is simply discarded by the broker.
			// However the publisher can tell the broker to keep the last message on that topic by setting the retained message flag.
			std::uint8_t RETAIN : 1;
			std::uint8_t QoS : 2;
			std::uint8_t DUP : 1;
			std::uint8_t : 4;
		}FieldPUBLISH;
		std::uint8_t Value = 0;
	}Data;

public:
	tFixedHeaderBase() = default;
	explicit tFixedHeaderBase(tControlPacketType controlPacketType)
	{
		Data.Field.ControlPacketType = static_cast<std::uint8_t>(controlPacketType);
		if (controlPacketType == tControlPacketType::PUBREL ||
			controlPacketType == tControlPacketType::SUBSCRIBE ||
			controlPacketType == tControlPacketType::UNSUBSCRIBE)
		{
			Data.Field.Flags = 0x02;
		}
	}
	explicit tFixedHeaderBase(std::uint8_t val)
	{
		Data.Value = val;
	}

	template<typename T>
	static std::optional<std::pair<T, std::size_t>> Parse(tSpan& data)
	{
		if (data.empty())
			return {};
		T FixedHeader(data[0]);
		const auto ControlPacketType = FixedHeader.GetControlPacketType();
		if (ControlPacketType < tControlPacketType::CONNECT || ControlPacketType > tControlPacketType::DISCONNECT)
			return {};
		data.Skip(1);
		auto RLengtOpt = tRemainingLength::Parse(data);
		if (!RLengtOpt.has_value() || *RLengtOpt > data.size())
			return {};
		return std::pair(FixedHeader, *RLengtOpt);
	}

	tControlPacketType GetControlPacketType() const { return static_cast<tControlPacketType>(Data.Field.ControlPacketType); }

	std::string ToString(bool align = false) const;
	std::string ToStringControlPacketType() const;

	std::vector<std::uint8_t> ToVector(std::size_t dataSize) const;

	bool operator==(const tFixedHeaderBase& val) const { return Data.Value == val.Data.Value; }
};

template<tControlPacketType ControlPacketType>
class tFixedHeaderBaseT : public tFixedHeaderBase
{
public:
	tFixedHeaderBaseT() :tFixedHeaderBase(ControlPacketType) {}
	explicit tFixedHeaderBaseT(std::uint8_t val)
	{
		Data.Value = val;
	}

	static std::optional<std::pair<tFixedHeaderBaseT, std::size_t>> Parse(tSpan& data)
	{
		return tFixedHeaderBase::Parse<tFixedHeaderBaseT>(data);
	}

	static tControlPacketType GetControlPacketType() { return ControlPacketType; }
};

template<>
class tFixedHeaderBaseT<tControlPacketType::PUBLISH> : public tFixedHeaderBase
{
public:
	tFixedHeaderBaseT() :tFixedHeaderBase(tControlPacketType::PUBLISH) {}
	tFixedHeaderBaseT(bool retain, tQoS qos, bool dup)
		:tFixedHeaderBase(tControlPacketType::PUBLISH)
	{
		Data.FieldPUBLISH.RETAIN = retain ? 1 : 0;
		Data.FieldPUBLISH.QoS = static_cast<std::uint8_t>(qos);
		Data.FieldPUBLISH.DUP = dup ? 1 : 0;
	}
	explicit tFixedHeaderBaseT(std::uint8_t val)
	{
		Data.Value = val;
	}

	static std::optional<std::pair<tFixedHeaderBaseT, std::size_t>> Parse(tSpan& data)
	{
		return tFixedHeaderBase::Parse<tFixedHeaderBaseT>(data);
	}

	static tControlPacketType GetControlPacketType() { return tControlPacketType::PUBLISH; }

	bool GetDUP() const { return Data.FieldPUBLISH.DUP; }
	tQoS GetQoS() const { return static_cast<tQoS>(Data.FieldPUBLISH.QoS); }
	bool GetRetain() const { return Data.FieldPUBLISH.RETAIN; }

	std::string ToString(bool align = false) const
	{
		std::string Str = tFixedHeaderBase::ToString(align);
		Str += " Retain: " + mqtt_main::ToString(GetRetain());
		Str += ", QoS: " + mqtt_main::ToString(GetQoS());
		Str += ", DUP: " + mqtt_main::ToString(GetDUP());
		return Str;
	}
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

	static std::optional<tPacketBase> Parse(tSpan& data)
	{
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

	static tControlPacketType GetControlPacketType() { return TCont::fixed_header_type::GetControlPacketType(); }

	TCont::fixed_header_type GetFixedHeader() const { return m_Content.FixedHeader; }
	TCont::variable_header_type GetVariableHeader() const { return m_Content.VariableHeader; }
	TCont::payload_type GetPayload() const { return m_Content.Payload; }

	std::string ToString() const override { return m_Content.ToString(); }
	std::string ToStringControlPacketType() const override { return m_Content.FixedHeader.ToStringControlPacketType(); }

	std::vector<std::uint8_t> ToVector() const override { return m_Content.ToVector(); }

	bool operator==(const tPacketBase& val) const { return m_Content == val.m_Content; }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// EMPTY

template<tControlPacketType ControlPacketType>
struct tContentEMPTY
{
	using tFixedHeader = tFixedHeaderBaseT<ControlPacketType>;

	tFixedHeader FixedHeader{};

	using fixed_header_type = tFixedHeader;
	using variable_header_type = void;
	using payload_type = void;

	tContentEMPTY() = default;

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
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CONNECT

struct tContentCONNECT
{
	using tFixedHeader = tFixedHeaderBaseT<tControlPacketType::CONNECT>;

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

	using fixed_header_type = tFixedHeader;
	using variable_header_type = tVariableHeader;
	using payload_type = tPayload;

	tContentCONNECT() = default;
	tContentCONNECT(tSessionStateRequest sessStateReq, std::uint16_t keepAlive, const std::string& clientId, tQoS willQos, bool willRetain, const std::string& willTopic, const std::string& willMessage, const std::string& userName, const std::string& password);
	tContentCONNECT(tSessionStateRequest sessStateReq, std::uint16_t keepAlive, const std::string& clientId, tQoS willQos, bool willRetain, const std::string& willTopic, const std::string& willMessage);
	tContentCONNECT(tSessionStateRequest sessStateReq, std::uint16_t keepAlive, const std::string& clientId);
	tContentCONNECT(tSessionStateRequest sessStateReq, std::uint16_t keepAlive, const std::string& clientId, const std::string& userName, const std::string& password);
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
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CONNACK

struct tContentCONNACK
{
	using tFixedHeader = tFixedHeaderBaseT<tControlPacketType::CONNACK>;

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

	using fixed_header_type = tFixedHeader;
	using variable_header_type = tVariableHeader;
	using payload_type = void;

	tContentCONNACK() = default;
	tContentCONNACK(tSessionState sessionState, tConnectReturnCode connectRetCode);

	static std::optional<tContentCONNACK> Parse(tSpan& data);

	std::string ToString() const;

	std::vector<std::uint8_t> ToVector() const;

	bool operator==(const tContentCONNACK& val) const = default;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLISH

struct tContentPUBLISH
{
	using tFixedHeader = tFixedHeaderBaseT<tControlPacketType::PUBLISH>;

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

	using fixed_header_type = tFixedHeader;
	using variable_header_type = tVariableHeader;
	using payload_type = std::vector<std::uint8_t>;

	tContentPUBLISH() = default;
	tContentPUBLISH(bool retain, const std::string& topicName);
	tContentPUBLISH(bool retain, const std::string& topicName, const std::vector<std::uint8_t>& payload);
	tContentPUBLISH(bool retain, bool dup, const std::string& topicName, tQoS qos, tUInt16 packetId);
	tContentPUBLISH(bool retain, bool dup, const std::string& topicName, tQoS qos, tUInt16 packetId, const std::vector<std::uint8_t>& payload);
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
	static bool IsPacketIdPresent(tQoS qos) { return static_cast<tQoS>(qos) != tQoS::AtMostOnceDelivery; }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PUBACK, PUBREC, PUBREL, PUBCOMP

template<tControlPacketType ControlPacketType>
struct tContentPID
{
	using tFixedHeader = tFixedHeaderBaseT<ControlPacketType>;

	tFixedHeader FixedHeader{};
	struct tVariableHeader
	{
		tUInt16 PacketId;

		bool operator==(const tVariableHeader& val) const = default;
	}VariableHeader;

	using fixed_header_type = tFixedHeader;
	using variable_header_type = tVariableHeader;
	using payload_type = void;

	tContentPID() = default;
	explicit tContentPID(tUInt16 packetId)
	{
		VariableHeader.PacketId = packetId;
	}

	static std::optional<tContentPID> Parse(tSpan& data)
	{
		std::optional<std::pair<tFixedHeader, std::size_t>> FixedHeaderOpt = tFixedHeader::Parse(data);
		if (!FixedHeaderOpt.has_value())
			return {};

		tContentPID Content{};
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

	bool operator==(const tContentPID& val) const
	{
		return FixedHeader == val.FixedHeader && VariableHeader == val.VariableHeader;
	}
};

using tContentPUBACK = tContentPID<tControlPacketType::PUBACK>;
using tContentPUBREC = tContentPID<tControlPacketType::PUBREC>;
using tContentPUBREL = tContentPID<tControlPacketType::PUBREL>;
using tContentPUBCOMP = tContentPID<tControlPacketType::PUBCOMP>;
using tContentUNSUBACK = tContentPID<tControlPacketType::UNSUBACK>;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SUBSCRIBE

struct tContentSUBSCRIBE
{
	using tFixedHeader = tFixedHeaderBaseT<tControlPacketType::SUBSCRIBE>;

	tFixedHeader FixedHeader{};
	struct tVariableHeader
	{
		tUInt16 PacketId;

		bool operator==(const tVariableHeader& val) const = default;
	}VariableHeader;

	struct tTopicFilter
	{
		tString TopicFilter;
		tQoS QoS = tQoS::AtMostOnceDelivery;

		static std::optional<tTopicFilter> Parse(tSpan& data);

		std::string ToString() const;

		std::vector<std::uint8_t> ToVector() const;

		bool operator==(const tTopicFilter&) const = default;
	};
	std::vector<tTopicFilter> Payload;

	using fixed_header_type = tFixedHeader;
	using variable_header_type = tVariableHeader;
	using payload_type = std::vector<tTopicFilter>;

	tContentSUBSCRIBE() = default;
	tContentSUBSCRIBE(tUInt16 packetId, const payload_type& topicFilters);
	tContentSUBSCRIBE(tUInt16 packetId, const tTopicFilter& topicFilter);

	static std::optional<tContentSUBSCRIBE> Parse(tSpan& data);

	std::string ToString() const;

	std::vector<std::uint8_t> ToVector() const;

	bool operator==(const tContentSUBSCRIBE& val) const = default;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SUBACK

struct tContentSUBACK
{
	using tFixedHeader = tFixedHeaderBaseT<tControlPacketType::SUBACK>;

	tFixedHeader FixedHeader{};
	struct tVariableHeader
	{
		tUInt16 PacketId;

		bool operator==(const tVariableHeader& val) const = default;
	}VariableHeader;

	std::vector<tSubscribeReturnCode> Payload;

	using fixed_header_type = tFixedHeader;
	using variable_header_type = tVariableHeader;
	using payload_type = std::vector<tSubscribeReturnCode>;

	tContentSUBACK() = default;
	tContentSUBACK(tUInt16 packetId, std::vector<tSubscribeReturnCode> payload);

	static std::optional<tContentSUBACK> Parse(tSpan& data);

	std::string ToString() const;

	std::vector<std::uint8_t> ToVector() const;

	bool operator==(const tContentSUBACK& val) const = default;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UNSUBSCRIBE

struct tContentUNSUBSCRIBE
{
	using tFixedHeader = tFixedHeaderBaseT<tControlPacketType::UNSUBSCRIBE>;

	tFixedHeader FixedHeader{};
	struct tVariableHeader
	{
		tUInt16 PacketId;

		bool operator==(const tVariableHeader& val) const = default;
	}VariableHeader;

	std::vector<tString> Payload;

	using fixed_header_type = tFixedHeader;
	using variable_header_type = tVariableHeader;
	using payload_type = std::vector<tString>;

	tContentUNSUBSCRIBE() = default;
	tContentUNSUBSCRIBE(tUInt16 packetId, const payload_type& topicFilters);
	tContentUNSUBSCRIBE(tUInt16 packetId, const tString& topicFilter);

	static std::optional<tContentUNSUBSCRIBE> Parse(tSpan& data);

	std::string ToString() const;

	std::vector<std::uint8_t> ToVector() const;

	bool operator==(const tContentUNSUBSCRIBE& val) const = default;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // hidden

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class tPacketNOACK
{
public:
	static std::optional<tPacketNOACK> Parse(tSpan& data) { return {}; }

	static tControlPacketType GetControlPacketType() { return tControlPacketType::_None; }

	std::string ToString() const { return {}; }

	std::vector<std::uint8_t> ToVector() const { return {}; }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class tPacketCONNACK : public hidden::tPacketBase<hidden::tContentCONNACK>
{
public:
	tPacketCONNACK() = delete;
	tPacketCONNACK(tSessionState sessionState, tConnectReturnCode connectRetCode)
		:tPacketBase(hidden::tContentCONNACK(sessionState, connectRetCode))
	{
	}
	tPacketCONNACK(const hidden::tPacketBase<hidden::tContentCONNACK>& val) :tPacketBase(val) {} // Parse(..) in the base class returns an instance of the base class and it shall be transform to an instance of derived one (for std::future<..>, std::optional<..>).
	tPacketCONNACK(hidden::tPacketBase<hidden::tContentCONNACK>&& val) :tPacketBase(std::move(val)) {} // Parse(..) in the base class returns an instance of the base class and it shall be transform to an instance of derived one (for std::future<..>, std::optional<..>).
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class tPacketCONNECT : public hidden::tPacketBase<hidden::tContentCONNECT>
{
public:
	using response_type = tPacketCONNACK;

	tPacketCONNECT() = delete;
	tPacketCONNECT(tSessionStateRequest sessionStateRequest, std::uint16_t keepAlive, const std::string& clientId, tQoS willQos, bool willRetain, const std::string& willTopic, const std::string& willMessage, const std::string& userName, const std::string& password)
		:tPacketBase(hidden::tContentCONNECT(sessionStateRequest, keepAlive, clientId, willQos, willRetain, willTopic, willMessage, userName, password))
	{
	}
	tPacketCONNECT(tSessionStateRequest sessionStateRequest, std::uint16_t keepAlive, const std::string& clientId, tQoS willQos, bool willRetain, const std::string& willTopic, const std::string& willMessage)
		:tPacketBase(hidden::tContentCONNECT(sessionStateRequest, keepAlive, clientId, willQos, willRetain, willTopic, willMessage))
	{
	}
	tPacketCONNECT(tSessionStateRequest sessionStateRequest, std::uint16_t keepAlive, const std::string& clientId, const std::string& userName, const std::string& password)
		:tPacketBase(hidden::tContentCONNECT(sessionStateRequest, keepAlive, clientId, userName, password))
	{
	}
	tPacketCONNECT(tSessionStateRequest sessionStateRequest, std::uint16_t keepAlive, const std::string& clientId)
		:tPacketBase(hidden::tContentCONNECT(sessionStateRequest, keepAlive, clientId))
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
	tPacketPUBACK(const hidden::tPacketBase<hidden::tContentPUBACK>& val) :tPacketBase(val) {} // Parse(..) in the base class returns an instance of the base class and it shall be transform to an instance of derived one (for std::future<..>, std::optional<..>).
	tPacketPUBACK(hidden::tPacketBase<hidden::tContentPUBACK>&& val) :tPacketBase(std::move(val)) {} // Parse(..) in the base class returns an instance of the base class and it shall be transform to an instance of derived one (for std::future<..>, std::optional<..>).
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
	tPacketPUBREC(const hidden::tPacketBase<hidden::tContentPUBREC>& val) :tPacketBase(val) {} // Parse(..) in the base class returns an instance of the base class and it shall be transform to an instance of derived one (for std::future<..>, std::optional<..>).
	tPacketPUBREC(hidden::tPacketBase<hidden::tContentPUBREC>&& val) :tPacketBase(std::move(val)) {} // Parse(..) in the base class returns an instance of the base class and it shall be transform to an instance of derived one (for std::future<..>, std::optional<..>).
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
	using response_type = tPacketNOACK;

	tPacketPUBLISH() = delete;
	tPacketPUBLISH(bool retain, const std::string& topicName)
		:tPacketBase(hidden::tContentPUBLISH(retain, topicName))
	{
	}
	tPacketPUBLISH(bool retain, const std::string& topicName, const std::vector<std::uint8_t>& payload)
		:tPacketBase(hidden::tContentPUBLISH(retain, topicName, payload))
	{
	}
};

template<>
class tPacketPUBLISH<tQoS::AtLeastOnceDelivery> : public hidden::tPacketBase<hidden::tContentPUBLISH>
{
public:
	using response_type = tPacketPUBACK;

	tPacketPUBLISH() = delete;
	tPacketPUBLISH(bool retain, bool dup, const std::string& topicName, tUInt16 packetId)
		:tPacketBase(hidden::tContentPUBLISH(dup, retain, topicName, tQoS::AtLeastOnceDelivery, packetId))
	{
	}
	tPacketPUBLISH(bool retain, bool dup, const std::string& topicName, tUInt16 packetId, const std::vector<std::uint8_t>& payload)
		:tPacketBase(hidden::tContentPUBLISH(retain, dup, topicName, tQoS::AtLeastOnceDelivery, packetId, payload))
	{
	}
};

template<>
class tPacketPUBLISH<tQoS::ExactlyOnceDelivery> : public hidden::tPacketBase<hidden::tContentPUBLISH>
{
public:
	using response_type = tPacketPUBREC;

	tPacketPUBLISH() = delete;
	tPacketPUBLISH(bool retain, bool dup, const std::string& topicName, tUInt16 packetId)
		:tPacketBase(hidden::tContentPUBLISH(retain, dup, topicName, tQoS::ExactlyOnceDelivery, packetId))
	{
	}
	tPacketPUBLISH(bool retain, bool dup, const std::string& topicName, tUInt16 packetId, const std::vector<std::uint8_t>& payload)
		:tPacketBase(hidden::tContentPUBLISH(retain, dup, topicName, tQoS::ExactlyOnceDelivery, packetId, payload))
	{
	}
};

using tPacketPUBLISH_Parse = tPacketPUBLISH<tQoS::AtMostOnceDelivery>;

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
	tPacketPUBCOMP(const hidden::tPacketBase<hidden::tContentPUBCOMP>& val) :tPacketBase(val) {} // Parse(..) in the base class returns an instance of the base class and it shall be transform to an instance of derived one (for std::future<..>, std::optional<..>).
	tPacketPUBCOMP(hidden::tPacketBase<hidden::tContentPUBCOMP>&& val) :tPacketBase(std::move(val)) {} // Parse(..) in the base class returns an instance of the base class and it shall be transform to an instance of derived one (for std::future<..>, std::optional<..>).
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// 879 A PUBREL Packet is the response to a PUBREC Packet.It is the third packet of the QoS 2 protocol
// 880 exchange.
class tPacketPUBREL : public hidden::tPacketBase<hidden::tContentPUBREL>
{
public:
	using response_type = tPacketPUBCOMP;

	tPacketPUBREL() = delete;
	explicit tPacketPUBREL(tUInt16 packetId)
		:tPacketBase(hidden::tContentPUBREL(packetId))
	{
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class tPacketSUBACK : public hidden::tPacketBase<hidden::tContentSUBACK>
{
public:
	tPacketSUBACK() = delete;
	tPacketSUBACK(tUInt16 packetId, const std::vector<tSubscribeReturnCode>& subscribeReturnCodes)
		:tPacketBase(hidden::tContentSUBACK(packetId, subscribeReturnCodes))
	{
	}
	tPacketSUBACK(const hidden::tPacketBase<hidden::tContentSUBACK>& val) :tPacketBase(val) {} // Parse(..) in the base class returns an instance of the base class and it shall be transform to an instance of derived one (for std::future<..>, std::optional<..>).
	tPacketSUBACK(hidden::tPacketBase<hidden::tContentSUBACK>&& val) :tPacketBase(std::move(val)) {} // Parse(..) in the base class returns an instance of the base class and it shall be transform to an instance of derived one (for std::future<..>, std::optional<..>).
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using tSubscribeTopicFilter = hidden::tContentSUBSCRIBE::tTopicFilter;

class tPacketSUBSCRIBE : public hidden::tPacketBase<hidden::tContentSUBSCRIBE>
{
public:
	using response_type = tPacketSUBACK;

	tPacketSUBSCRIBE() = delete;
	tPacketSUBSCRIBE(tUInt16 packetId, const std::vector<tSubscribeTopicFilter>& topicFilters)
		:tPacketBase(hidden::tContentSUBSCRIBE(packetId, topicFilters))
	{
	}
	tPacketSUBSCRIBE(tUInt16 packetId, const tSubscribeTopicFilter& topicFilter)
		:tPacketBase(hidden::tContentSUBSCRIBE(packetId, topicFilter))
	{
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class tPacketUNSUBACK : public hidden::tPacketBase<hidden::tContentUNSUBACK>
{
public:
	tPacketUNSUBACK() = delete;
	explicit tPacketUNSUBACK(tUInt16 packetId)
		:tPacketBase(hidden::tContentUNSUBACK(packetId))
	{
	}
	tPacketUNSUBACK(const hidden::tPacketBase<hidden::tContentUNSUBACK>& val) :tPacketBase(val) {} // Parse(..) in the base class returns an instance of the base class and it shall be transform to an instance of derived one (for std::future<..>, std::optional<..>).
	tPacketUNSUBACK(hidden::tPacketBase<hidden::tContentUNSUBACK>&& val) :tPacketBase(std::move(val)) {} // Parse(..) in the base class returns an instance of the base class and it shall be transform to an instance of derived one (for std::future<..>, std::optional<..>).
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class tPacketUNSUBSCRIBE : public hidden::tPacketBase<hidden::tContentUNSUBSCRIBE>
{
public:
	using response_type = tPacketUNSUBACK;

	tPacketUNSUBSCRIBE() = delete;
	tPacketUNSUBSCRIBE(tUInt16 packetId, const std::vector<tString>& topicFilters)
		:tPacketBase(hidden::tContentUNSUBSCRIBE(packetId, topicFilters))
	{
	}
	tPacketUNSUBSCRIBE(tUInt16 packetId, const tString& topicFilter)
		:tPacketBase(hidden::tContentUNSUBSCRIBE(packetId, topicFilter))
	{
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class tPacketPINGRESP : public hidden::tPacketBase<hidden::tContentEMPTY<tControlPacketType::PINGRESP>>
{
public:
	tPacketPINGRESP() :tPacketBase(hidden::tContentEMPTY<tControlPacketType::PINGRESP>()) {}
	tPacketPINGRESP(const hidden::tPacketBase<hidden::tContentEMPTY<tControlPacketType::PINGRESP>>& val) :tPacketBase(val) {} // Parse(..) in the base class returns an instance of the base class and it shall be transform to an instance of derived one (for std::future<..>, std::optional<..>).
	tPacketPINGRESP(hidden::tPacketBase<hidden::tContentEMPTY<tControlPacketType::PINGRESP>>&& val) :tPacketBase(std::move(val)) {} // Parse(..) in the base class returns an instance of the base class and it shall be transform to an instance of derived one (for std::future<..>, std::optional<..>).
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class tPacketPINGREQ : public hidden::tPacketBase<hidden::tContentEMPTY<tControlPacketType::PINGREQ>>
{
public:
	using response_type = tPacketPINGRESP;

	tPacketPINGREQ() :tPacketBase(hidden::tContentEMPTY<tControlPacketType::PINGREQ>()) {}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class tPacketDISCONNECT : public hidden::tPacketBase<hidden::tContentEMPTY<tControlPacketType::DISCONNECT>>
{
public:
	using response_type = tPacketNOACK;

	tPacketDISCONNECT() :tPacketBase(hidden::tContentEMPTY<tControlPacketType::DISCONNECT>()) {}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using tPacketDataSpan = std::pair<tControlPacketType, tSpan>;

std::optional<tPacketDataSpan> TestPacket(tSpan& data);
std::optional<tPacketDataSpan> TestPacket(const std::vector<std::uint8_t>& data);

}
}
}
