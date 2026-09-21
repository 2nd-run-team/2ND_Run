# 2ND_Run

![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.8.2-0E1128?logo=unrealengine)
![License](https://img.shields.io/badge/license-MIT-green)

Unreal Engine **5.8.2** 프로젝트.

## 요구 사항

| 항목 | 버전 | 비고 |
|------|------|------|
| Unreal Engine | **5.8.2** | 팀 전원 동일 패치 버전 고정 |
| Git LFS | 3.x 이상 | [설치](https://git-lfs.com) |
| Visual Studio | 2022 (17.8 이상) | C++ 작업 시 "Game development with C++" 워크로드 |

### 엔진 버전을 고정하는 이유

상위 버전 에디터로 저장한 에셋은 **하위 버전에서 열리지 않습니다.** 한 명이
5.8.2로 저장한 `.uasset`을 5.8.0 쓰는 팀원이 열면 로드에 실패하고, 되돌릴
방법이 없습니다. 업그레이드는 반드시 팀 전체가 같은 시점에 합니다.

에픽 런처 → 라이브러리에서 설치된 버전을 확인하세요. 5.8.0/5.8.1이면 5.8.2로
업데이트하면 됩니다. 버전을 올릴 때는 이 표와 상단 배지를 함께 갱신해
PR로 공유합니다.

> 5.8은 UE5 계열의 마지막 메이저 릴리스입니다. UE6는 2027년 말 얼리 액세스가
> 목표라, 이 프로젝트 기간 동안은 5.8.x 안에서 패치 버전만 관리하면 됩니다.

## 시작하기

```bash
git lfs install                                          # 반드시 clone 전에
git clone https://github.com/2nd-run-team/2ND_Run.git
cd 2ND_Run
git config lfs.locksverify true
```

> `git lfs install` 을 먼저 하지 않으면 `.uasset` 이 100바이트짜리 포인터
> 텍스트로 내려와 프로젝트 전체가 깨져 보입니다. 가장 흔한 초기 사고입니다.

`.uproject` 우클릭 → *Generate Visual Studio project files* (C++ 프로젝트인 경우)
→ `.sln` 빌드 → 에디터 실행.

## 폴더 구조

```
Content/      에셋 (LFS 관리, 락 필요)
Config/       프로젝트 설정 — 변경 시 팀 공유 필수
Source/       C++ 소스 (C++ 프로젝트인 경우)
Plugins/      플러그인
```

`Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` 는 각자 로컬에서
생성되는 폴더라 저장소에 올리지 않습니다.

## 협업 규칙

에셋 작업 전 **반드시 LFS 락을 먼저 잡으세요.** `.uasset`/`.umap` 은 머지가
불가능해서 동시 수정 시 한쪽 작업이 사라집니다.

자세한 내용은 [CONTRIBUTING.md](CONTRIBUTING.md) 참고.

## 팀

이 저장소는 [`2nd-run-team`](https://github.com/2nd-run-team) 조직 소유입니다.
개인 계정이 아니라 조직에 귀속되어 있어, 구성원이 바뀌어도 소유권 이전이
필요 없습니다.

| 팀 | 저장소 권한 | 담당 | 멤버 |
|----|------------|------|------|
| `@2nd-run-team/programmers` | Write | `Source/`, `Config/` | |
| `@2nd-run-team/artists` | Write | `Content/` | |
| `@2nd-run-team/planners` | Triage | 이슈 · 일정 | |

Write = 브랜치 푸시, PR 생성·리뷰. `main`/`develop` 직접 푸시는 보호 규칙이 차단합니다.
Triage = 이슈·PR 관리 (코드 푸시 불가).

## 라이선스

[MIT](LICENSE)

주의: MIT는 이 저장소의 **코드**에 적용됩니다. 마켓플레이스 에셋, 외부에서
받아온 모델·사운드·폰트 등은 각자의 원 라이선스를 그대로 따르며 MIT로
재배포되지 않습니다. 재배포 제한이 있는 에셋은 커밋 전에 확인하세요.
