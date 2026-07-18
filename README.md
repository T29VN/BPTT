# BPTT - Bình Phương Tối Thiểu (python), ESP32 (C)

Dự án này triển khai phép hồi quy đa thức bậc 2 bằng phương pháp bình phương tối thiểu và có kèm theo file code C nạp cho esp 32 và link web để điều khiển esp32 thông qua firebase "https://da220252.vercel.app/". Mã nguồn web "https://github.com/T29VN/WEB_DA2.git"

```text
y = a*x^2 + b*x + c
```

Mã nguồn chính nằm trong thư mục `lsm_calculation_project/`. Chương trình đọc dữ liệu từ file CSV có hai cột `x` và `y`, tính các hệ số `a`, `b`, `c`, sau đó xuất báo cáo dạng TXT và có thể tạo biểu đồ PNG.

## Cấu Trúc Thư Mục

```text
BPTT/
├── .claude/                         # Cấu hình/kỹ năng cho agent
├── lsm_calculation_project/
│   ├── data/
│   │   ├── du_lieu_sau_xu_ly.csv    # Dữ liệu mặc định
│   │   └── Du_lieu_moi.csv          # Dữ liệu thay thế, dùng qua --data
│   ├── docs/diagrams/               # Tài liệu và sơ đồ draw.io
│   ├── output/
│   │   ├── equation_result.txt      # Báo cáo kết quả
│   │   └── bieu_do_fit.png          # Biểu đồ fit và phần dư
│   ├── src/
│   │   ├── main.py                  # CLI và pipeline chính
│   │   ├── lsm_solver.py            # Thuật toán bình phương tối thiểu
│   │   └── visualizer.py            # Tạo biểu đồ bằng matplotlib
│   └── tests/
│       ├── test_algorithm.py        # Custom test runner
│       └── tinh_phuong_trinh_bac_2.py
├── requirements.txt                 # Dependency cho project
├── README.md                        # File giới thiệu tổng quan này
├── AGENTS.md
├── CLAUDE.md
└── analysis_report.md
```

## Chức Năng Chính

- Đọc file CSV có hai cột `x` và `y`.
- Fit đa thức bậc 2 theo mô hình `y = a*x^2 + b*x + c`.
- Dùng `fractions.Fraction` trong các bước tính chính để giữ độ chính xác tốt hơn so với tính trực tiếp bằng `float`.
- Tính các chỉ số thống kê như residuals, SSE, SSR, SST, R², adjusted R², MSE, MSR, F-statistic, standard error và t-statistics.
- Ghi báo cáo chi tiết ra `lsm_calculation_project/output/equation_result.txt`.
- Tạo biểu đồ tổng hợp ra `lsm_calculation_project/output/bieu_do_fit.png` nếu không dùng tùy chọn `--no-chart`.

## Cài Đặt

Từ thư mục gốc `BPTT/`, cài dependency bằng:

```powershell
pip install -r requirements.txt
```

Các dependency hiện tại:

```text
matplotlib>=3.5
numpy>=1.23
```

Trong đó:

- `matplotlib` được dùng bởi `src/visualizer.py` để tạo biểu đồ.
- `numpy` được khai báo vì script phụ `tests/tinh_phuong_trinh_bac_2.py` đang import `numpy`.

## Cách Chạy Chương Trình

Chạy với dữ liệu mặc định `data/du_lieu_sau_xu_ly.csv`:

```powershell
cd lsm_calculation_project
python src/main.py
```

Chạy với file dữ liệu thay thế:

```powershell
cd lsm_calculation_project
python src/main.py --data data/Du_lieu_moi.csv
```

Chỉ xuất báo cáo TXT, không tạo biểu đồ:

```powershell
cd lsm_calculation_project
python src/main.py --no-chart
```

Chỉ định thư mục output khác:

```powershell
cd lsm_calculation_project
python src/main.py --output-dir output
```

## Dữ Liệu Đầu Vào

File CSV cần có header `x,y`, ví dụ:

```csv
x,y
0,0.943715846995
1,0.945081967213
2,0.939016393443
```

Trong project hiện có hai file dữ liệu:

- `lsm_calculation_project/data/du_lieu_sau_xu_ly.csv`: dữ liệu mặc định. Bộ dữ liệu này tạo ra bộ hệ số `a`, `b`, `c` hiện đang được hardcode trong script phụ `tests/tinh_phuong_trinh_bac_2.py`.
- `lsm_calculation_project/data/Du_lieu_moi.csv`: dữ liệu thay thế, chỉ được dùng khi truyền qua tùy chọn `--data`.

Không nên tự gán ý nghĩa vật lý cho `x`, `y`, `TO` hoặc `TA` nếu chưa có tài liệu riêng xác nhận.

## Kết Quả Đầu Ra

Sau khi chạy thành công, chương trình có thể tạo:

- `lsm_calculation_project/output/equation_result.txt`: báo cáo chi tiết gồm hệ số hồi quy, phương trình, ANOVA, R², adjusted R², F-statistic, sai số và bảng so sánh.
- `lsm_calculation_project/output/bieu_do_fit.png`: biểu đồ gồm dữ liệu thực, đường fit và phần dư.
- Console stdout: bản tóm tắt kết quả in trực tiếp ra màn hình.

## Kiểm Thử

File `lsm_calculation_project/tests/test_algorithm.py` là custom test runner tự viết, không phải pytest hoặc unittest.

Chạy kiểm thử:

```powershell
cd lsm_calculation_project
python tests/test_algorithm.py
```

Bộ kiểm thử hiện có kiểm tra:

- Dữ liệu đã biết với nghiệm kỳ vọng.
- Pipeline với file CSV mặc định.
- Một số lỗi validation.
- Tính nhất quán của các chỉ số thống kê.

## Tài Liệu Và Sơ Đồ

Sơ đồ hệ thống draw.io nằm tại:

```text
lsm_calculation_project/docs/diagrams/source/BPTT_system_diagrams.drawio
```

File này gồm các trang:

- Kiến trúc tổng quan.
- Luồng thực thi chính.
- Bình phương tối thiểu và thống kê.
- Khử Gauss.
- Phạm vi kiểm thử.

Trạng thái tiếp tục công việc sơ đồ được ghi tại:

```text
lsm_calculation_project/docs/diagrams/resume_state.md
```

## Lưu Ý Khi Phát Triển

- Không chạy `main.py` nếu bạn không muốn ghi đè `output/equation_result.txt` và `output/bieu_do_fit.png`.
- Không sửa các file output đã tạo nếu cần giữ nguyên kết quả hiện tại.
- Nên kiểm tra file dữ liệu đầu vào trước khi so sánh hệ số `a`, `b`, `c`, vì hai CSV hiện có cho ra hai bộ hệ số khác nhau.
- Khi cập nhật tài liệu, ưu tiên phản ánh đúng mã local hiện tại thay vì dựa vào nguồn bên ngoài.

