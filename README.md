# mini-tcp-dos-lab

Một bài lab TCP cục bộ dành cho người mới học Linux Network Programming. Project cho thấy một server blocking xử lý từng client ra sao, vì sao một client gửi request chậm có thể giữ server tại `recv()`, và cách receive timeout giới hạn thời gian chờ.

Lab chỉ bind vào `127.0.0.1`. Các Python client cũng cố định địa chỉ này; `slow_client.py` chỉ cho phép tối đa 10 kết nối. Đây là mô phỏng nhỏ để học resource exhaustion ở tầng ứng dụng, không phải công cụ kiểm thử hệ thống từ xa.

## 1. Project overview

Server hiểu một dòng lệnh cho mỗi TCP connection:

| Lệnh | Phản hồi |
| --- | --- |
| `HELLO` | `Hello from mini TCP server!` |
| `TIME` | Thời gian hiện tại của máy server |
| `QUIT` | `Goodbye!`, sau đó đóng connection |

Request phải kết thúc bằng newline (`\n`). TCP là luồng byte, không giữ ranh giới giữa các lần `send()` của client và `recv()` của server. Vì vậy server đọc thành từng phần cho đến khi thấy newline, rồi mới xử lý lệnh.

## 2. Architecture

```text
Python Client
      |
      | TCP over 127.0.0.1
      v
+-------------+
|  C Server   |
+-------------+
      |
      v
 Linux Kernel
```

Server chạy tuần tự: `accept()` một client, đọc và trả lời request đó, đóng connection, rồi mới gọi `accept()` lần nữa. `listen()` dùng backlog nhỏ để kernel xếp một số connection đang chờ; nó không khiến server xử lý đồng thời.

## 3. Build

```bash
make
```

Makefile chạy `gcc -Wall -Wextra -std=c11 src/server.c -o server`. `make clean` xóa executable được build.

### What you should understand: Makefile

- `all` là target mặc định; target `server` biên dịch `src/server.c` thành `server`.
- Dòng lệnh trong target phải bắt đầu bằng ký tự tab.
- `run` bắt đầu server ở port 8080; `clean` chỉ xóa file build.

## 4. Start server

Chạy chế độ blocking, chưa bật timeout:

```bash
./server 8080
```

Server chỉ lắng nghe trên `127.0.0.1:8080`. Dừng server bằng `Ctrl+C`.

## 5. Normal test

Trong terminal khác:

```bash
python3 tools/normal_client.py
```

Nhập `HELLO`, `TIME`, hoặc `QUIT`. Có thể truyền port khác, ví dụ `python3 tools/normal_client.py 9090`; địa chỉ host vẫn luôn là `127.0.0.1`.

### What you should understand: `normal_client.py`

- `socket.create_connection()` tạo TCP connection tới loopback và có thời hạn chờ 5 giây.
- `sendall()` gửi đủ request đã mã hóa thành byte và thêm newline.
- Client đọc response đến khi server đóng connection. Mỗi lệnh mở một connection mới vì server phục vụ một lệnh rồi đóng.
- `HOST` là hằng số, không có tùy chọn nhập địa chỉ Internet.

## 6. Slow client experiment

Trong terminal thứ nhất, chạy server ở chế độ mặc định. Trong terminal thứ hai:

```bash
python3 tools/slow_client.py
```

Mặc định script mở một connection và gửi `HELLO\n` từng byte, cách nhau 2 giây. Trong lúc script đang gửi, thử chạy `python3 tools/normal_client.py` ở terminal thứ ba và nhập `HELLO`. Client bình thường đã kết nối nhưng chưa được server xử lý. Sau khi slow client gửi newline và được phục vụ xong, server mới nhận client tiếp theo.

Có thể mở một số connection nhỏ để quan sát connection đang chờ:

```bash
python3 tools/slow_client.py --connections 3 --delay 2
```

Giới hạn cứng là 10 connection; script giữ các connection phụ mở đến khi bạn nhấn Enter. Chúng không gửi request. Không tăng giới hạn hoặc dùng script ngoài loopback.

### What you should understand: `slow_client.py`

- `socket.create_connection()` luôn kết nối tới `127.0.0.1`; số connection bị giới hạn từ 1 đến 10.
- `sendall()` gửi một byte của `HELLO\n` mỗi lần. `time.sleep()` tạo khoảng trống giữa các byte để server phải chờ newline.
- Chỉ connection đầu tiên gửi dữ liệu; connection còn lại được giữ mở để bạn quan sát hàng đợi và trạng thái TCP.
- Với `--delay 6` ở chế độ phòng thủ, server có thể timeout trước byte tiếp theo. Delay nhỏ hơn 5 giây có thể tiếp tục giữ server do timeout đặt lại ở mỗi lần `recv()`.

## 7. Observe Blocking I/O

Trong `src/server.c`, luồng xử lý là:

```text
socket -> bind -> listen -> accept -> recv -> process -> send -> close
```

Server nhận một client bằng `accept()`, sau đó `handle_client()` lặp `recv()` đến khi nhận newline. Nếu client gửi một phần request rồi dừng, lần `recv()` kế tiếp chờ dữ liệu. Vì server chưa quay lại vòng lặp chính để gọi `accept()`, client khác không được xử lý. Đây là ví dụ resource exhaustion ở tầng ứng dụng: kết nối hợp lệ nhưng tiêu thụ khả năng phục vụ hữu hạn của một server tuần tự.

Một File Descriptor (FD) là số nguyên tiến trình dùng để tham chiếu một tài nguyên đang mở. `socket()` trả FD cho listening socket; mỗi `accept()` trả FD riêng cho client. `close()` giải phóng FD đó. Server này không có thread hay event loop.

## 8. Monitor server

Trong terminal khác:

```bash
./tools/monitor.sh
```

Script tìm tiến trình tên `server` mới nhất, rồi hiển thị PID, CPU, RSS memory, số connection TCP `ESTABLISHED` có local port tương ứng và số symlink trong `/proc/<PID>/fd`. Có thể chỉ rõ PID và port: `./tools/monitor.sh 4217 8080`. Số liệu connection là snapshot; connection đang nằm trong backlog cũng có thể xuất hiện trong `ss` dù ứng dụng chưa gọi `accept()`.

### What you should understand: `monitor.sh`

- `ps` đọc thông tin tiến trình; RSS được kernel báo theo KiB và script đổi sang MB.
- `ss` đọc trạng thái TCP từ kernel; bộ lọc `sport` đếm socket có local/source port của server.
- `/proc/<PID>/fd` cho thấy các FD mà Linux đang giữ cho tiến trình; `find` và `wc` đếm chúng.
- `pgrep` tự tìm PID nếu không truyền đối số. Có thể truyền PID trực tiếp khi chạy nhiều server.

## 9. Enable defense

Dừng server blocking bằng `Ctrl+C`, sau đó chạy:

```bash
./server 8080 --timeout
```

Server đặt `SO_RCVTIMEO` là 5 giây trên từng client socket đã `accept()`. Khi `recv()` không nhận thêm byte nào trong thời gian đó, Linux trả lỗi `EAGAIN`/`EWOULDBLOCK`; server báo timeout, đóng connection và quay lại `accept()`.

```text
Trước: accept -> recv chờ vô hạn -> client khác không được xử lý
Sau:   accept -> recv chờ tối đa 5 giây -> close client -> accept tiếp
```

Thử cho client chờ quá timeout trước byte kế tiếp:

```bash
python3 tools/slow_client.py --delay 6
```

### What you should understand: `src/server.c`

- `socket(AF_INET, SOCK_STREAM, 0)`: tạo IPv4 TCP socket. `AF_INET` chọn địa chỉ IPv4; `SOCK_STREAM` chọn luồng tin cậy có thứ tự của TCP; `0` chọn protocol mặc định. Thành công trả về FD không âm, lỗi trả `-1`.
- `bind()`: gắn FD với địa chỉ và port cục bộ. `sockaddr_in` chứa họ địa chỉ, port ở network byte order (`htons`) và loopback (`htonl(INADDR_LOOPBACK)`). Thành công trả `0`, lỗi trả `-1`.
- `listen()`: chuyển socket đã bind sang trạng thái chờ connection; backlog là hàng đợi giới hạn cho connection đang chờ được ứng dụng nhận. Trả `0` khi thành công, `-1` khi lỗi.
- `accept()`: lấy một connection khỏi hàng đợi và trả về FD mới. FD listening vẫn mở để nhận client sau; `accept()` blocking khi hàng đợi trống.
- `recv()`: đọc byte từ client FD. Giá trị dương là số byte đọc được, `0` là peer đóng kết nối, `-1` là lỗi. TCP có thể chia một request thành nhiều lần `recv()`.
- `send()`: gửi byte qua client FD; có thể gửi ít byte hơn yêu cầu nên helper lặp đến khi gửi hết. `MSG_NOSIGNAL` tránh server bị kết thúc bởi SIGPIPE nếu client đóng sớm.
- `setsockopt(..., SO_RCVTIMEO, ...)`: cấu hình thời gian chờ cho thao tác nhận trên client FD. Nó không làm server concurrent; chỉ giúp server tuần tự thoát khỏi một lần chờ dữ liệu nhàn rỗi.
- `close()`: giải phóng FD client sau khi xử lý. `SO_REUSEADDR` là tùy chọn socket giúp dễ khởi động lại server sau khi dừng.

Timeout này là **idle timeout cho mỗi lần `recv()`**, không phải tổng thời gian tối đa cho cả request. Slow client gửi từng byte cách nhau dưới 5 giây vẫn có thể giữ server lâu hơn 5 giây, vì mỗi lần nhận được dữ liệu thì lần `recv()` kế tiếp bắt đầu chờ lại. Đây là defense nhập môn, không phải biện pháp đầy đủ cho server production.

## 10. What I learned

- TCP socket programming bằng C: `socket()`, `bind()`, `listen()`, `accept()`, `recv()`, `send()`, `close()`.
- Blocking I/O, TCP stream framing và File Descriptor cơ bản.
- Timeout nhận dữ liệu và giới hạn của timeout đơn giản.
- Khái niệm resource exhaustion / application-layer DoS trong server tuần tự.
- Python socket programming, Bash, `ps`, `ss`, `/proc` và Makefile cơ bản.

## Learning phases

1. **Phase 1 - Minimal TCP server:** đọc `src/server.c`, build, chạy `./server 8080`, và theo dõi FD listening cùng FD client.
2. **Phase 2 - Normal Python client:** gửi `HELLO`, `TIME`, `QUIT`; thử nhập một lệnh không hợp lệ.
3. **Phase 3 - Blocking I/O:** mở client kết nối nhưng chưa gửi newline, rồi quan sát server chưa phục vụ client khác.
4. **Phase 4 - Slow client lab:** chạy `slow_client.py` và quan sát request được gửi từng byte.
5. **Phase 5 - System resources:** dùng `monitor.sh` trong lúc có client đang giữ hoặc chờ được phục vụ.
6. **Phase 6 - Receive timeout:** khởi động lại với `--timeout`, thử `--delay 6`, rồi so sánh hành vi.
7. **Phase 7 - Explain it back:** không nhìn README, vẽ lại đường đi của FD và giải thích vì sao newline khiến server gọi `recv()` nhiều lần; sau đó nêu giới hạn của idle timeout khi byte đến cách nhau dưới 5 giây.