# BPTT - Binh Phuong Toi Thieu

Du an nay tinh hoi quy da thuc bac 2 bang phuong phap binh phuong toi thieu:

```text
y = a*x^2 + b*x + c
```

Ma nguon chinh nam trong `lsm_calculation_project/`. Chuong trinh doc du lieu CSV gom hai cot `x` va `y`, tinh cac he so `a`, `b`, `c`, tao bao cao ket qua dang TXT va co the xuat bieu do PNG.

## Cau truc thu muc

```text
BPTT/
├── .claude/                         # Cau hinh/ky nang cho agent
├── lsm_calculation_project/
│   ├── data/
│   │   ├── du_lieu_sau_xu_ly.csv    # Du lieu mac dinh
│   │   └── Du_lieu_moi.csv          # Du lieu thay the, dung qua --data
│   ├── docs/diagrams/               # Tai lieu va so do draw.io
│   ├── output/
│   │   ├── equation_result.txt      # Bao cao ket qua
│   │   └── bieu_do_fit.png          # Bieu do fit va phan du
│   ├── src/
│   │   ├── main.py                  # CLI va pipeline chinh
│   │   ├── lsm_solver.py            # Thuat toan binh phuong toi thieu
│   │   └── visualizer.py            # Tao bieu do bang matplotlib
│   ├── tests/
│   │   ├── test_algorithm.py        # Custom test runner
│   │   └── tinh_phuong_trinh_bac_2.py
│   ├── requirements.txt
│   └── README.md
├── AGENTS.md
├── CLAUDE.md
└── analysis_report.md
```

## Chuc nang chinh

- Doc file CSV co hai cot `x` va `y`.
- Fit da thuc bac 2 `y = a*x^2 + b*x + c`.
- Su dung `fractions.Fraction` trong cac buoc tinh chinh de giam sai so lam tron.
- Tinh cac chi so thong ke: residuals, SSE, SSR, SST, R^2, adjusted R^2, MSE, MSR, F-statistic, standard error va t-statistics.
- Ghi bao cao chi tiet ra `output/equation_result.txt`.
- Tao bieu do tong hop ra `output/bieu_do_fit.png` neu khong dung `--no-chart`.

## Cai dat

```powershell
cd lsm_calculation_project
pip install -r requirements.txt
```

Dependency hien tai:

```text
matplotlib>=3.5
```

## Cach chay

Chay voi du lieu mac dinh `data/du_lieu_sau_xu_ly.csv`:

```powershell
cd lsm_calculation_project
python src/main.py
```

Chi dinh file du lieu khac:

```powershell
python src/main.py --data data/Du_lieu_moi.csv
```

Chi xuat bao cao TXT, khong tao bieu do:

```powershell
python src/main.py --no-chart
```

Chi dinh thu muc output:

```powershell
python src/main.py --output-dir output
```

## Du lieu dau vao

File CSV can co header:

```csv
x,y
0,0.943715846995
1,0.945081967213
2,0.939016393443
```

Trong project hien co:

- `du_lieu_sau_xu_ly.csv`: du lieu mac dinh. Bo du lieu nay tao ra bo he so `a`, `b`, `c` hien dang duoc hardcode trong script phu `tests/tinh_phuong_trinh_bac_2.py`.
- `Du_lieu_moi.csv`: du lieu thay the, chi duoc dung khi truyen qua tuy chon `--data`.

Khong nen gan y nghia vat ly cho `x`, `y`, `TO` hoac `TA` neu khong co tai lieu rieng xac nhan.

## Kiem thu

File `tests/test_algorithm.py` la custom test runner tu viet, khong phai pytest hay unittest.

Chay test:

```powershell
cd lsm_calculation_project
python tests/test_algorithm.py
```

Bo test hien co kiem tra:

- Du lieu da biet voi nghiem ky vong.
- Pipeline voi file CSV mac dinh.
- Mot so loi validation.
- Tinh nhat quan cua cac chi so thong ke.

## Tai lieu so do

So do he thong draw.io nam tai:

```text
lsm_calculation_project/docs/diagrams/source/BPTT_system_diagrams.drawio
```

File nay gom cac trang:

- Kien truc tong quan.
- Luong thuc thi chinh.
- Binh phuong toi thieu va thong ke.
- Khu Gauss.
- Pham vi kiem thu.

Trang thai tiep tuc cong viec so do duoc ghi trong:

```text
lsm_calculation_project/docs/diagrams/resume_state.md
```

## Luu y an toan khi phat trien

- Khong chay `main.py` neu ban khong muon ghi de `output/equation_result.txt` va `output/bieu_do_fit.png`.
- Khong sua cac file output da tao neu can giu nguyen ket qua hien tai.
- Nen kiem tra du lieu dau vao truoc khi so sanh he so `a`, `b`, `c`, vi hai CSV hien co cho ra hai bo he so khac nhau.
