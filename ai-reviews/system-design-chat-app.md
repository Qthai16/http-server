Dưới đây là cách mình sẽ clarify và design.

## 1. Câu hỏi clarify

**Product scope**

* Chat 1-1 hay group chat? Group tối đa bao nhiêu người?
* Có support multi-device không?
* Có cần message edit/delete/reaction/reply/thread/pin không?
* Có cần typing indicator, online status, read receipt, delivered receipt không?
* Có cần media/file/voice message không?
* Có cần search message không?
* Có cần E2EE như Signal/Telegram secret chat không, hay chỉ TLS + encryption at rest?

**Scale & SLA**

* DAU/MAU dự kiến?
* Peak concurrent connections?
* QPS gửi message?
* Latency mục tiêu: p50/p95/p99?
* Message ordering yêu cầu mạnh tới đâu?
* Retention bao lâu?
* Region: single-region hay multi-region?
* Availability target: 99.9, 99.99?

**QoS**

* Offline user có nhận lại đầy đủ không?
* Client reconnect có cần resume từ sequence number không?
* Có đảm bảo exactly-once không, hay at-least-once + dedupe?
* Push notification có bắt buộc không?
* Message priority: normal vs call invite vs system message?

---

## 2. High-level architecture

```
Client
  |
  | WebSocket / QUIC / MQTT-like persistent connection
  v
Gateway / Realtime Edge
  |
  v
Auth + Session Service
  |
  v
Message Ingest Service
  |
  +--> Message Store
  +--> Conversation Metadata Store
  +--> Fanout Queue / Stream
              |
              v
        Delivery Workers
              |
              +--> Online delivery via Gateway
              +--> Offline push notification
              +--> Sync cursor update
```

Core idea: **write message durably first, then deliver asynchronously**.

---

## 3. Database design

### Message store

Dùng Cassandra/ScyllaDB/DynamoDB-like nếu scale lớn.

Partition theo `conversation_id`, sort theo `message_seq` hoặc timestamp.

```sql
messages_by_conversation (
  conversation_id,
  message_seq,
  message_id,
  sender_id,
  created_at,
  message_type,
  body_ciphertext,
  metadata,
  PRIMARY KEY (conversation_id, message_seq)
)
```

Không nên chỉ dùng timestamp để order vì collision và clock skew. Mỗi conversation nên có **monotonic sequence number**.

### Conversation metadata

```sql
conversations (
  conversation_id,
  type, -- direct/group
  created_at,
  last_message_seq,
  last_message_preview,
  updated_at
)
```

### Members

```sql
conversation_members (
  conversation_id,
  user_id,
  role,
  joined_at,
  last_read_seq,
  last_delivered_seq,
  muted_until
)
```

### Inbox index

Để list chat nhanh:

```sql
user_inbox (
  user_id,
  updated_at,
  conversation_id,
  last_message_seq,
  unread_count,
  pinned,
  archived
)
```

---

## 4. Send message flow

1. Client gửi `client_msg_id`, `conversation_id`, payload.
2. Gateway authenticate token.
3. Message service check membership.
4. Generate `message_id`.
5. Allocate `message_seq` per conversation.
6. Persist message.
7. Update conversation + inbox index.
8. Publish event vào Kafka/Pulsar/NATS stream.
9. Delivery workers fanout tới online recipients.
10. Client ack bằng `message_id` / `message_seq`.

Client nên gửi `client_msg_id` để server dedupe khi retry.

---

## 5. Push message to client

### Protocol

Mình sẽ chọn:

* **WebSocket** cho baseline: phổ biến, dễ debug, browser/mobile support tốt.
* **QUIC/WebTransport** nếu muốn optimize mobile network switching.
* **gRPC streaming** nếu internal service-to-service.

Message frame có dạng:

```json
{
  "type": "message.new",
  "conversation_id": "c123",
  "message_id": "m456",
  "seq": 1024,
  "sender_id": "u1",
  "payload": "...",
  "server_time": 1710000000
}
```

Client maintain cursor:

```json
{
  "conversation_id": "c123",
  "last_seen_seq": 1023
}
```

Khi reconnect, client gọi sync:

```http
GET /conversations/{id}/messages?after_seq=1023
```

---

## 6. Delivery semantics / QoS

Mình sẽ không promise exactly-once end-to-end. Thực tế nên dùng:

**At-least-once delivery + idempotent client dedupe.**

Cơ chế:

* Server có `message_id`.
* Client dedupe theo `message_id`.
* Client ACK khi nhận.
* Server retry nếu chưa ACK.
* Client sync lại bằng cursor khi reconnect.

Các trạng thái:

* `sent`: server đã persist.
* `delivered`: recipient device đã ACK receive.
* `read`: user mở conversation tới seq đó.

Multi-device:

* Delivery ACK có thể per-device.
* Read receipt nên per-user, không per-device.

---

## 7. Ordering

Trong một conversation:

* Server cấp `message_seq` tăng dần.
* Client render theo `message_seq`.
* Nếu nhận thiếu seq, client trigger gap sync.

Cross-conversation:

* Không cần global ordering.
* Inbox sort theo `updated_at` hoặc logical timestamp.

Group chat lớn:

* Small group: fanout-on-write.
* Huge group/channel: fanout-on-read hoặc hybrid.

---

## 8. Encryption

### Transit encryption

Bắt buộc:

* TLS 1.2+ hoặc TLS 1.3.
* Certificate pinning trên mobile nếu threat model cần.
* mTLS cho internal service-to-service nếu hệ thống nhạy cảm.

### At-rest encryption

* Encrypt disk / volume.
* Sensitive fields encrypt bằng KMS-managed keys.
* Media files lưu object storage, encrypt at rest.

### End-to-end encryption

Nếu cần E2EE:

* Server chỉ lưu ciphertext.
* Key exchange theo Signal Double Ratchet hoặc tương đương.
* Group E2EE phức tạp hơn: sender keys, device key management, recovery, multi-device sync.
* Search server-side sẽ khó hoặc không làm được trên plaintext.

Nếu không yêu cầu E2EE, baseline là TLS + encryption at rest + strict access control.

---

## 9. Media/file message

Không gửi file qua WebSocket.

Flow:

1. Client xin upload URL.
2. Upload trực tiếp lên object storage.
3. Server scan virus/content policy nếu cần.
4. Message chỉ chứa `media_id`, metadata, thumbnail.
5. Recipient download qua signed URL.

---

## 10. Offline push notification

Khi user offline:

* Message vẫn persist.
* Delivery worker gửi push qua APNs/FCM.
* Push payload không nên chứa nội dung nhạy cảm nếu có privacy requirement.
* Client mở app thì sync bằng cursor, không tin hoàn toàn vào push.

---

## 11. Presence / typing

Không lưu DB bền vững.

Dùng Redis / in-memory presence service:

* `user_id -> active connections`
* TTL heartbeat 20–60s
* typing event ephemeral, expire sau vài giây

Không cần delivery guarantee mạnh cho typing/presence.

---

## 12. Monitoring

Metrics quan trọng:

* WebSocket active connections.
* Message send QPS.
* End-to-end delivery latency p50/p95/p99.
* Persist latency.
* Queue lag.
* Fanout failure rate.
* Reconnect rate.
* ACK timeout rate.
* Push notification success/failure.
* DB hot partition.
* Dropped connection by region/app version.

Logs:

* Không log plaintext message.
* Log `message_id`, `conversation_id`, `user_id` hash, latency, error code.

Tracing:

* Trace từ `send_message` → DB write → queue → delivery → ACK.

Alert:

* Queue lag tăng.
* Delivery latency p99 vượt ngưỡng.
* Gateway connection drop spike.
* DB write error.
* Push provider failure.

---

## 13. Reliability

Important patterns:

* Idempotency key: `client_msg_id`.
* Durable queue between write and delivery.
* Backpressure ở gateway.
* Rate limit per user/conversation.
* Circuit breaker với push providers.
* Dead-letter queue cho event lỗi.
* Replay stream để recover delivery worker.

---

## 14. Security / abuse

* Auth bằng short-lived access token.
* Refresh token riêng.
* Check membership mỗi lần gửi message.
* Rate limit spam.
* Block/report user.
* Anti-abuse pipeline cho link/media.
* Audit log cho admin access.
* Data deletion/export nếu có compliance requirement.

---

## 15. Một thiết kế baseline mình sẽ chọn

Nếu cần production-ready cho app kiểu Messenger/Telegram:

* Client ↔ Gateway: **WebSocket over TLS**
* Internal event bus: **Kafka/Pulsar**
* Message DB: **ScyllaDB/Cassandra/DynamoDB**
* Metadata/index: **PostgreSQL hoặc MySQL**, nếu scale vừa; tách sang KV/NoSQL khi lớn
* Cache/presence: **Redis**
* Media: **Object storage + CDN**
* Push: **FCM/APNs**
* Delivery: **at-least-once + client dedupe**
* Ordering: **per-conversation monotonic sequence**
* Encryption: **TLS + encryption at rest**, optional E2EE nếu product yêu cầu

Thiết kế này thực dụng vì nó tách rõ 3 việc: **persist**, **fanout**, và **sync/recovery**. Đây là điểm sống còn của realtime chat: đừng để WebSocket delivery là source of truth; source of truth phải là message store + cursor.
