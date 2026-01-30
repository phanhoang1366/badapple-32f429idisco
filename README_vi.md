# Bad Apple!! trên STM32F429I-DISC1

*[English version](README.md)*

Dự án này triển khai hoạt hình "Bad Apple!!" nổi tiếng trên bo mạch phát triển STM32F429I-DISC1. Hoạt hình được hiển thị trên màn hình LCD tích hợp, thể hiện khả năng của vi điều khiển dòng STM32F4.

### Nhóm và vai trò thành viên

Tên nhóm: NoMMU

| STT | Họ và tên | Mã sinh viên | Vai trò |
|-----|-----------|--------------|---------|
| 1 | Phan Huy Hoàng | 20235729 | Triển khai và tối ưu codec tùy chỉnh, viết encoder cho video |
| 2 | Đặng Tiến Cường | 20220020 | Triển khai tua cho video và âm thanh |
| 3 | Trịnh Tuấn Thành | 20225532 | Lập trình video, triển khai DAC và ADPCM, hiển thị/ẩn lời bài hát |

### Tính năng

- Phát video 160x120, 15FPS trên màn hình ILI9341 tích hợp của STM32F429I-DISC1
- Phát âm thanh ADPCM 4000Hz qua DAC của STM32
- Hỗ trợ các nút bấm ngoài để sử dụng các tính năng bổ sung: Phát/Tạm dừng, Tua tới/lùi 5 giây và hiển thị/ẩn lời bài hát.

### Phần cứng bổ sung

- Bo mạch 4 nút bấm
- Bo mạch khuếch đại LM386
- Mạch RC để lọc tín hiệu

## Cấu hình sơ đồ (trên CubeMX)

| STM32F429 | Nút bấm | Bộ khuếch đại |
|-----------|---------|---------------|
| PA0 | Nút người dùng mặc định cho Phát/Tạm dừng | |
| PA5 | | Đầu ra DAC |
| PE2 | Nút ngoài để hiển thị/ẩn lời bài hát | |
| PE3 | Nút ngoài để tua tới | |
| PE4 | Nút ngoài để tua lùi | |
| PE5 | Nút ngoài cho Phát/Tạm dừng | |
| 5V | | VCC |
| GND | GND | GND |

## Triển khai Video
Pipeline video được xây dựng xung quanh luồng bit RLE nhỏ gọn cho từng khung hình và bộ render LCD đệm kép. Mỗi khung hình được mã hóa offline thành định dạng tùy chỉnh với ba loại khung hình.

### K-frame (0x00)
Ảnh chụp toàn bộ khung hình. Mỗi dòng quét được nén RLE với pixel 1-bit và độ dài run tối đa 127. Loại này được sử dụng định kỳ làm điểm làm mới sạch hoặc khi delta frame lớn hơn keyframe. Nó cung cấp thời gian giải mã nhanh và dự đoán được nhưng với payload lớn hơn.

### I-frame (0x01)
Khung hình delta chỉ cập nhật các hàng đã thay đổi. Một bitmap 15 byte đánh dấu hàng nào trong số 120 hàng đã được sửa đổi (1 bit cho mỗi hàng). Chỉ những hàng đó mới được mã hóa RLE và vá. Khi giải mã, khung hình trước đó được sao chép trước, sau đó các hàng được đánh dấu được phủ lên, giữ băng thông thấp và chi phí giải mã nhỏ.

### D-frame (0x02)
Khung hình trùng lặp được sử dụng khi hình ảnh hiện tại giống hệt với hình ảnh trước đó. Nó chỉ chứa byte loại khung hình và không có payload. Bộ giải mã chỉ đơn giản tái sử dụng nội dung framebuffer cuối cùng, làm cho nó trở thành khung hình nhỏ nhất có thể.

Khi chạy, firmware tra cứu offset byte của khung hình từ bảng `frame_offset` được tính toán trước, giải mã khung hình và ghi vào back framebuffer. Một thao tác swap đồng bộ VSync (`screen_flip_buffers()`) làm cho khung hình mới hiển thị trong khi tránh hiện tượng tearing. Điều này giữ cho việc giải mã nhanh và giảm băng thông flash trong khi duy trì 15 FPS ở độ phân giải 160×120.

Xem [thư mục encoder](encoder/) nếu bạn muốn sử dụng video khác. Video nên có độ tương phản rất cao để có ích.

## Âm thanh

ADPCM Mono 4000Hz được sử dụng để phát âm thanh. [Thư mục encoder](encoder/) cũng có hướng dẫn để tạo âm thanh của riêng bạn.

## Lời bài hát

Chúng tôi đã sử dụng và chỉnh sửa lời bài hát từ [video này](https://www.youtube.com/watch?v=UsIAaRLUI9s) để giúp căn chỉnh tốt hơn với hai ngôn ngữ.

## Tua tới/lùi

Video này có keyframe được đặt vào mỗi giây để giúp việc tua không bị lỗi frame. Vì ADPCM là hệ có nhớ, chúng tôi cũng thêm vào các trạng thái của ADPCM theo từng giây để reset về đúng trạng thái.

## Hạn chế đã biết

- Việc triển khai video chưa đủ hiệu quả vì luôn có chỗ để cải thiện. Ví dụ, có thể triển khai nhiều loại khung hình hơn, đặt chiều rộng và chiều cao vào đầu video, và các phương pháp nén khác có thể hoạt động tốt hơn.

- Đầu vào cảm ứng chưa được triển khai, vì bo mạch của chúng tôi không có màn hình cảm ứng hoạt động đúng cách.

## Ghi nhận

Dự án này sử dụng triển khai encoder/decoder ADPCM được cung cấp bởi STMicroelectronics (© 2009 STMicroelectronics), ban đầu được phân phối như một phần của các ví dụ firmware STM32.
Mã nguồn chỉ được sử dụng cho mục đích giáo dục.
