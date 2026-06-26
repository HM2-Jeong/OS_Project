# ✈️ SkyPort 체크인 스케줄러
> **Operating Systems Term Project** — 공항 체크인 카운터 멀티레벨 CPU 스케줄링 시뮬레이터

---

## 📌 Overview

공항 체크인 카운터 환경(5개 카운터, 3등급 승객)을 CPU 스케줄링 문제로 모델링하여,  
**Average Turnaround Time(ATT)** 을 최소화하는 Hybrid MLFQ 스케줄러를 설계·구현한 프로젝트입니다.

| OS 개념 | 체크인 시스템 대응 |
|---|---|
| CPU | 체크인 카운터 (5개) |
| Process | 승객 (50명) |
| Burst Time | 서비스 시간 |
| Priority | 승객 등급 (FIRST > BUSINESS > ECONOMY) |
| Ready Queue | 등급별 분리 큐 |

---

## 🏗️ 스케줄링 설계 (Proposed Strategy)

### 큐 아키텍처

```
FIRST    → Q1 (FCFS)  → C1 (First 전용)
BUSINESS → Q2 (HRRN)  → C2 (Business 전용)
                         C5 (Flex: B→E)
ECONOMY  → Q3 (SPN)   → C3 (Economy 전용)
                         C4 (Flex: E→B)

Promotion: Q3 대기 > 20 unit → Q2로 승격 (기아 방지)
```

### 알고리즘 선택 근거

| 큐 | 알고리즘 | 근거 |
|---|---|---|
| Q1 (First, 8명) | **FCFS** | 승객 수 적고 서비스 시간 편차 작음 → 예측 가능한 서비스 제공 |
| Q2 (Business, 10명) | **HRRN** | 대기 시간 비례 우선순위로 긴 작업 starvation 방지 |
| Q3 (Economy, 32명) | **SPN (SJF)** | 최대 규모 등급, 짧은 작업 우선으로 전체 ATT 최소화 |

### 카운터 배정 전략

- **C1~C3 (전용)**: 타 등급 허용 ❌ — 등급별 서비스 가용성 보장
- **C4 (Flex)**: Economy 우선 → Business fallback
- **C5 (Flex)**: Business 우선 → Economy fallback  
  *(First 제외: 서비스 시간이 길어 Convoy Effect 유발)*

---

## 🔧 구현

### 시스템 아키텍처

```
input.txt
    ↓
Input Parser
    ↓
Scheduler Engine (Context)
    ├── Q1 First  [FCFS]
    ├── Q2 Business [HRRN]   ←── Promotion (대기 > 20 unit)
    └── Q3 Economy [SPN]
    ↓
Counters (C1~C5)
    ↓
Result Analyzer
```

**Strategy Pattern** 적용 — 알고리즘 교체 시 엔진 코드 수정 불필요

```
ISchedulerStrategy (interface)
├── ProposedStrategy   — Hybrid MLFQ
├── BaselineA_FCFS     — 단순 FCFS
├── BaselineB_Priority — 등급 고정 우선순위
└── BaselineC_SJF      — Non-preemptive SJF
```

### 핵심 모듈

**Promotion Logic** (기아 방지)
```cpp
function checkPromotion():
    for each process in Q3_Economy:
        waiting_time = current_time - process.arrival_time
        if waiting_time > 20 and process.is_promoted == false:
            process.is_promoted = true
            move process to Q2_Business
```

**Counter Assignment** (Proposed)
```
C1 → Q1 (FCFS)
C2 → Q2 (HRRN)
C3 → Q3 (SPN)
C4 → Q3 (SPN) → fallback Q2 (HRRN)
C5 → Q2 (HRRN) → fallback Q3 (SPN)
```

---

## ⚙️ 빌드 및 실행

**요구사항**: C++11 이상, g++ 또는 MSVC

```bash
# 1. OS.cpp 와 input.txt 를 같은 디렉토리에 위치
# 2. 컴파일
g++ -o OS OS.cpp

# 3. 실행
./OS
```

**input.txt 형식** (공백 구분):
```
[passenger_id] [arrival_time] [class(1/2/3)] [service_time]
```

---

## 📊 결과

### ATT 비교

| 스케줄러 | ATT | Baseline 대비 |
|---|---|---|
| **Proposed (Hybrid MLFQ)** | **17.40** | 기준 |
| Baseline A: FCFS | 19.50 | ↓ 10.77% 개선 |
| Baseline B: Priority FCFS | 22.50 | ↓ 22.67% 개선 |
| Baseline C: Non-preemptive SJF | 14.90 | ↑ 16.78% 악화 |

### 등급별 ATT (Proposed)

| 등급 | 승객 수 | 평균 ATT |
|---|---|---|
| First | 8 | 37.12 |
| Business | 11 | 19.36 |
| Economy | 31 | 11.61 |
| **전체** | **50** | **17.40** |

### 카운터 가동률

| 카운터 | 유형 | 처리 승객 | 총 처리시간 | 유휴시간 |
|---|---|---|---|---|
| C1 | First 전용 | 8 | 102 | 0 |
| C2 | Business 전용 | 8 | 72 | 30 |
| C3 | Economy 전용 | 14 | 73 | 29 |
| C4 | Flex (E→B) | 12 | 68 | 34 |
| C5 | Flex (B→E) | 8 | 64 | 38 |

> 전체 가동률: **74.3%** (사용 379 / 가용 510)

---

## ⚖️ Trade-off 분석

- **First Class ATT 역전**: C1 단일 전용 카운터 구조로 인해 First(37.12) > Economy(11.61) 역전 발생
- **Flex 카운터 제한**: C4·C5에서 First 제외 → 일부 상황에서 자원 활용 비효율
- **Promotion 임계값 고정**: 20 unit 임계값이 입력 패턴에 따라 최적이 아닐 수 있음

---

## 👥 역할 분담

| 이름 | 역할 |
|---|---|
| 김수빈 | 보고서 작성 / 코드 구현 / PPT 제작·발표 |
| 정하밈 | 보고서 작성 / 코드 구현 / PPT 제작·발표 |

---

## 🤖 생성형 AI 활용

| AI | 활용 목적 |
|---|---|
| ChatGPT | 알고리즘 구조 검토 및 논리 오류 분석 |
| Claude | 초기 코드 구조 및 객체지향 설계 보조 |
| Gemini | 결과 해석 및 보고서 문장 정리 |

> AI 출력을 그대로 사용하지 않고, 전공 지식 기반으로 직접 검증·수정하는 과정을 거쳤습니다.

