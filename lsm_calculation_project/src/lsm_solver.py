"""
Module: lsm_solver.py
Phuong phap Binh phuong toi thieu (Least Squares Method)

Tim cac he so a, b, c trong phuong trinh:
    y = a*x^2 + b*x + c

DO CHINH XAC CAO: Su dung fractions.Fraction (so huu ti chinh xac tuyet doi)
    - Doc CSV dang string -> Fraction (khong qua float, khong mat precision)
    - Moi phep +, -, *, / deu chinh xac 100%, khong co sai so lam tron
    - Chi convert sang Decimal o cuoi de hien thi voi nhieu chu so

Tham khao:
    - Montgomery, D.C. et al. "Introduction to Linear Regression Analysis"
    - Chapra, S.C. "Numerical Methods for Engineers"
"""

import csv
import math
from pathlib import Path
from fractions import Fraction
from decimal import Decimal, getcontext

# Do chinh xac Decimal khi hien thi ket qua (50 chu so co nghia)
DO_CHINH_XAC = 50
getcontext().prec = DO_CHINH_XAC


# ============================================================
#  TIEN ICH: CHUYEN DOI FRACTION -> DECIMAL
# ============================================================

def fraction_sang_decimal(phan_so, so_chu_so=None):
    """
    Chuyen Fraction sang Decimal voi do chinh xac cao.

    Parameters
    ----------
    phan_so : Fraction
    so_chu_so : int, optional
        So chu so co nghia (mac dinh: DO_CHINH_XAC).

    Returns
    -------
    Decimal
    """
    if so_chu_so is None:
        so_chu_so = DO_CHINH_XAC
    saved = getcontext().prec
    getcontext().prec = so_chu_so + 10
    result = Decimal(phan_so.numerator) / Decimal(phan_so.denominator)
    getcontext().prec = saved
    return result


# ============================================================
#  1. DOC VA KIEM TRA DU LIEU
# ============================================================

def doc_du_lieu_csv(duong_dan_file):
    """
    Doc du lieu (x, y) tu file CSV.

    QUAN TRONG: Doc gia tri dang STRING roi chuyen truc tiep sang Fraction.
    Dieu nay dam bao khong co sai so lam tron do chuyen qua float.
        Fraction("1.0021021") = 10021021/10000000  (chinh xac tuyet doi)
        Fraction(float("1.0021021")) = ... (co sai so lam tron!)

    Parameters
    ----------
    duong_dan_file : str hoac Path

    Returns
    -------
    x_data : list[Fraction]
    y_data : list[Fraction]
    """
    duong_dan = Path(duong_dan_file)
    if not duong_dan.exists():
        raise FileNotFoundError(f"Khong tim thay file: {duong_dan}")

    x_data = []
    y_data = []

    with open(duong_dan, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)

        if reader.fieldnames is None:
            raise ValueError("File CSV khong co header!")

        ten_cot = [col.strip().lower() for col in reader.fieldnames]
        if "x" not in ten_cot or "y" not in ten_cot:
            raise ValueError(
                f"File CSV phai co cac cot 'x' va 'y', "
                f"nhung chi co {reader.fieldnames}"
            )

        for so_dong, row in enumerate(reader, start=2):
            x_str = row["x"].strip()
            y_str = row["y"].strip()

            # Kiem tra gia tri hop le bang float truoc
            try:
                x_float = float(x_str)
                y_float = float(y_str)
            except ValueError as e:
                raise ValueError(f"Loi du lieu tai dong {so_dong}: {e}")

            if math.isnan(x_float) or math.isinf(x_float):
                raise ValueError(
                    f"Gia tri x khong hop le tai dong {so_dong}: {x_str}"
                )
            if math.isnan(y_float) or math.isinf(y_float):
                raise ValueError(
                    f"Gia tri y khong hop le tai dong {so_dong}: {y_str}"
                )

            # Chuyen STRING -> Fraction (khong qua float!)
            x_data.append(Fraction(x_str))
            y_data.append(Fraction(y_str))

    if len(x_data) == 0:
        raise ValueError("File CSV khong co du lieu!")

    return x_data, y_data


def kiem_tra_du_lieu(x_data, y_data, bac_da_thuc=2):
    """
    Kiem tra tinh hop le cua du lieu truoc khi fit.

    Parameters
    ----------
    x_data : list[Fraction]
    y_data : list[Fraction]
    bac_da_thuc : int
    """
    so_he_so = bac_da_thuc + 1

    if len(x_data) != len(y_data):
        raise ValueError(
            f"So luong x ({len(x_data)}) va y ({len(y_data)}) khong khop!"
        )

    if len(x_data) < so_he_so:
        raise ValueError(
            f"Can it nhat {so_he_so} diem du lieu de fit da thuc bac "
            f"{bac_da_thuc}, nhung chi co {len(x_data)} diem!"
        )

    # cái này kiểm tra các x có trùng nhau không
    if len(set(x_data)) < so_he_so:
        raise ValueError(
            f"Can it nhat {so_he_so} gia tri x khac nhau, "
            f"nhung chi co {len(set(x_data))} gia tri phan biet!"
        )


# ============================================================
#  2. TINH CAC TONG (Fraction — chinh xac tuyet doi)
# ============================================================

def tinh_cac_tong(x_data, y_data):
    """
    Tinh cac tong can thiet de xay dung he phuong trinh chuan tac.
    Tat ca phep tinh dung Fraction -> khong co sai so tich luy.

    Parameters
    ----------
    x_data : list[Fraction]
    y_data : list[Fraction]

    Returns
    -------
    dict voi cac gia tri Fraction
    """
    n = len(x_data)
    ZERO = Fraction(0)
    Sx = Sx2 = Sx3 = Sx4 = ZERO
    Sy = Sxy = Sx2y = ZERO

    for i in range(n):
        xi = x_data[i]
        yi = y_data[i]

        xi2 = xi * xi
        xi3 = xi2 * xi
        xi4 = xi3 * xi

        Sx   += xi
        Sx2  += xi2
        Sx3  += xi3
        Sx4  += xi4
        Sy   += yi
        Sxy  += xi * yi
        Sx2y += xi2 * yi

    return {
        'n': n,
        'Sx': Sx, 'Sx2': Sx2, 'Sx3': Sx3, 'Sx4': Sx4,
        'Sy': Sy, 'Sxy': Sxy, 'Sx2y': Sx2y,
    }


# ============================================================
#  3. LAP HE PHUONG TRINH CHUAN TAC
# ============================================================

def lap_he_phuong_trinh_chuan_tac(cac_tong):
    """
    Lap he phuong trinh chuan tac tu dieu kien cuc tri.

    He:
        | Sx4   Sx3   Sx2 | |a|   | Sx2y |
        | Sx3   Sx2   Sx  | |b| = | Sxy  |
        | Sx2   Sx    n   | |c|   | Sy   |

    Parameters
    ----------
    cac_tong : dict (Fraction values)

    Returns
    -------
    A : list[list[Fraction]] — ma tran 3x3
    B : list[Fraction] — vecto ve phai
    """
    n    = Fraction(cac_tong['n'])
    Sx   = cac_tong['Sx']
    Sx2  = cac_tong['Sx2']
    Sx3  = cac_tong['Sx3']
    Sx4  = cac_tong['Sx4']
    Sy   = cac_tong['Sy']
    Sxy  = cac_tong['Sxy']
    Sx2y = cac_tong['Sx2y']

    A = [
        [Sx4, Sx3, Sx2],
        [Sx3, Sx2, Sx ],
        [Sx2, Sx,  n  ],
    ]

    B = [Sx2y, Sxy, Sy]

    return A, B


# ============================================================
#  4. GIAI HE PHUONG TRINH BANG PHUONG PHAP KHU GAUSS
# ============================================================

def giai_he_gauss(A, B):
    """
    Giai he A * x = B bang khu Gauss voi partial pivoting.
    Voi Fraction: tat ca phep chia deu chinh xac, khong lam tron.

    Parameters
    ----------
    A : list[list[Fraction]]
    B : list[Fraction]

    Returns
    -------
    list[Fraction] — nghiem chinh xac [a, b, c]
    """
    n = len(B)

    M = []
    for i in range(n):
        hang = [A[i][j] for j in range(n)]
        hang.append(B[i])
        M.append(hang)

    # Khu xuoi (Forward Elimination)
    for cot in range(n):
        gia_tri_max = abs(M[cot][cot])
        hang_max = cot

        for hang in range(cot + 1, n):
            if abs(M[hang][cot]) > gia_tri_max:
                gia_tri_max = abs(M[hang][cot])
                hang_max = hang

        if hang_max != cot:
            M[cot], M[hang_max] = M[hang_max], M[cot]

        # Voi Fraction, kiem tra bang 0 chinh xac (khong can tolerance)
        if M[cot][cot] == 0:
            raise ValueError(
                "Ma tran he so suy bien (det = 0), khong the giai he!"
            )

        for hang in range(cot + 1, n):
            he_so = M[hang][cot] / M[cot][cot]
            for j in range(cot, n + 1):
                M[hang][j] -= he_so * M[cot][j]

    # Thay nguoc (Back Substitution)
    nghiem = [Fraction(0)] * n

    for i in range(n - 1, -1, -1):
        nghiem[i] = M[i][n]
        for j in range(i + 1, n):
            nghiem[i] -= M[i][j] * nghiem[j]
        nghiem[i] /= M[i][i]

    return nghiem


# ============================================================
#  5. NGHICH DAO MA TRAN (Fraction — chinh xac)
# ============================================================

def nghich_dao_ma_tran(A):
    """
    Nghich dao ma tran NxN bang Gauss-Jordan. Fraction -> chinh xac.

    Parameters
    ----------
    A : list[list[Fraction]]

    Returns
    -------
    list[list[Fraction]] hoac None
    """
    n = len(A)

    M = []
    for i in range(n):
        hang = [A[i][j] for j in range(n)]
        for j in range(n):
            hang.append(Fraction(1) if i == j else Fraction(0))
        M.append(hang)

    for cot in range(n):
        hang_max = cot
        gia_tri_max = abs(M[cot][cot])
        for hang in range(cot + 1, n):
            if abs(M[hang][cot]) > gia_tri_max:
                gia_tri_max = abs(M[hang][cot])
                hang_max = hang

        if hang_max != cot:
            M[cot], M[hang_max] = M[hang_max], M[cot]

        pivot = M[cot][cot]
        if pivot == 0:
            return None

        for j in range(2 * n):
            M[cot][j] /= pivot

        for hang in range(n):
            if hang == cot:
                continue
            he_so = M[hang][cot]
            for j in range(2 * n):
                M[hang][j] -= he_so * M[cot][j]

    A_inv = []
    for i in range(n):
        A_inv.append([M[i][j] for j in range(n, 2 * n)])

    return A_inv


# ============================================================
#  6. TINH CAC CHI SO THONG KE HOI QUY
# ============================================================

def tinh_thong_ke(x_data, y_data, y_du_doan, he_so, A_matrix):
    """
    Tinh day du cac chi so thong ke. Dung Fraction cho moi thu
    tru math.sqrt (can chuyen sang float).

    Parameters
    ----------
    x_data, y_data, y_du_doan : list[Fraction]
    he_so : list[Fraction]
    A_matrix : list[list[Fraction]]

    Returns
    -------
    dict — cac chi so thong ke (Fraction + float cho SE)
    """
    n = len(y_data)
    p = len(he_so)
    df_reg = p - 1
    df_res = n - p
    df_total = n - 1

    y_tb = sum(y_data) / n

    sai_so = [y_data[i] - y_du_doan[i] for i in range(n)]

    SSE = sum(e * e for e in sai_so)
    SST = sum((yi - y_tb) ** 2 for yi in y_data)
    SSR = SST - SSE

    # R^2, Adjusted R^2 — Fraction (chinh xac tuyet doi)
    R2 = Fraction(1) - SSE / SST if SST > 0 else Fraction(0)

    if df_res > 0 and df_total > 0 and SST > 0:
        R2_adj = Fraction(1) - (SSE / df_res) / (SST / df_total)
    else:
        R2_adj = Fraction(0)

    MSE = SSE / df_res if df_res > 0 else Fraction(0)
    MSR = SSR / df_reg if df_reg > 0 else Fraction(0)

    # F-statistic — Fraction
    F_stat = MSR / MSE if MSE > 0 else None

    # SE can math.sqrt -> chuyen sang float tai day
    SE_estimate = math.sqrt(float(MSE))

    SE_he_so = [0.0] * p
    A_inv = nghich_dao_ma_tran(A_matrix)
    if A_inv is not None:
        for i in range(p):
            phuong_sai = float(MSE) * float(A_inv[i][i])
            SE_he_so[i] = math.sqrt(abs(phuong_sai))

    t_stats = []
    for i in range(p):
        if SE_he_so[i] > 1e-30:
            t_stats.append(float(he_so[i]) / SE_he_so[i])
        else:
            t_stats.append(float('inf'))

    # Phan tich phan du — Fraction
    sai_so_abs = [abs(e) for e in sai_so]
    sai_so_phan_tram = []
    for i in range(n):
        if abs(y_data[i]) > 0:
            sai_so_phan_tram.append(abs(sai_so[i] / y_data[i]) * 100)
        else:
            sai_so_phan_tram.append(Fraction(0))

    return {
        'sai_so': sai_so,
        'sai_so_phan_tram': sai_so_phan_tram,
        'SSE': SSE, 'SSR': SSR, 'SST': SST,
        'MSE': MSE, 'MSR': MSR,
        'R2': R2, 'R2_adj': R2_adj,
        'SE_estimate': SE_estimate,
        'SE_he_so': SE_he_so,
        't_stats': t_stats,
        'F_stat': F_stat,
        'df_reg': df_reg, 'df_res': df_res, 'df_total': df_total,
        'sai_so_tb': sum(sai_so) / n,
        'sai_so_abs_max': max(sai_so_abs),
        'sai_so_abs_tb': sum(sai_so_abs) / n,
        'sai_so_pt_max': max(sai_so_phan_tram),
        'sai_so_pt_tb': sum(sai_so_phan_tram) / n,
    }


# ============================================================
#  7. DINH DANG PHUONG TRINH
# ============================================================

def dinh_dang_phuong_trinh(a, b, c, so_chu_so=15):
    """
    Tao chuoi phuong trinh voi do chinh xac cao.
    Chuyen Fraction -> Decimal de hien thi nhieu chu so.
    """
    def fmt(val):
        d = fraction_sang_decimal(val, so_chu_so + 5)
        # Dung format g de bo cac so 0 thua
        formatted = format(d, f'.{so_chu_so}g')
        return formatted

    parts = [f"y = {fmt(a)}*x^2"]

    if b >= 0:
        parts.append(f" + {fmt(b)}*x")
    else:
        parts.append(f" - {fmt(abs(b))}*x")

    if c >= 0:
        parts.append(f" + {fmt(c)}")
    else:
        parts.append(f" - {fmt(abs(c))}")

    return "".join(parts)


# ============================================================
#  8. HAM CHINH: BINH PHUONG TOI THIEU (FULL PIPELINE)
# ============================================================

def binh_phuong_toi_thieu(duong_dan_csv):
    """
    Thuc hien toan bo quy trinh binh phuong toi thieu.
    Su dung Fraction cho moi buoc -> ket qua chinh xac tuyet doi.

    Parameters
    ----------
    duong_dan_csv : str hoac Path

    Returns
    -------
    dict — ket qua tong hop (Fraction + Decimal)
    """
    # Buoc 1: Doc du lieu -> Fraction
    x_data, y_data = doc_du_lieu_csv(duong_dan_csv)

    # Buoc 2: Kiem tra du lieu
    kiem_tra_du_lieu(x_data, y_data, bac_da_thuc=2)

    # Buoc 3: Tinh cac tong (Fraction)
    cac_tong = tinh_cac_tong(x_data, y_data)

    # Buoc 4: Lap he phuong trinh chuan tac (Fraction)
    A, B = lap_he_phuong_trinh_chuan_tac(cac_tong)

    # Buoc 5: Giai he phuong trinh (Fraction -> chinh xac tuyet doi)
    nghiem = giai_he_gauss(A, B)
    a, b, c = nghiem[0], nghiem[1], nghiem[2]

    # Buoc 6: Tinh gia tri du doan (Fraction)
    y_du_doan = [a * xi * xi + b * xi + c for xi in x_data]

    # Buoc 7: Thong ke (Fraction ngoai tru SE can sqrt)
    thong_ke = tinh_thong_ke(x_data, y_data, y_du_doan, nghiem, A)

    # Buoc 8: Chuyen sang Decimal de hien thi nhieu chu so
    a_dec = fraction_sang_decimal(a, 40)
    b_dec = fraction_sang_decimal(b, 40)
    c_dec = fraction_sang_decimal(c, 40)

    phuong_trinh = dinh_dang_phuong_trinh(a, b, c, so_chu_so=15)

    return {
        # Fraction (chinh xac tuyet doi)
        'a': a, 'b': b, 'c': c,
        # Decimal (40 chu so co nghia)
        'a_decimal': a_dec, 'b_decimal': b_dec, 'c_decimal': c_dec,
        # Phan so chinh xac
        'a_fraction': str(a), 'b_fraction': str(b), 'c_fraction': str(c),
        'phuong_trinh': phuong_trinh,
        'x_data': x_data, 'y_data': y_data,
        'y_du_doan': y_du_doan,
        'thong_ke': thong_ke,
        'so_diem': len(x_data),
        # Backward compatibility (float)
        'sai_so': thong_ke['sai_so'],
        'SSE': thong_ke['SSE'],
        'R2': thong_ke['R2'],
    }
