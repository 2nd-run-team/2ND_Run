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

### 1단계 — 조직 초대 수락

https://github.com/orgs/2nd-run-team/invitations 에서 초대를 수락합니다.
**초대는 7일 뒤 만료**되고, 수락 전에는 푸시 권한이 없습니다.

"클론은 되는데 푸시가 안 된다"면 대부분 이걸 안 한 경우입니다.

### 2단계 — Git LFS 설치 (clone 전에)

```bash
git lfs install
```

이걸 **먼저** 하지 않고 clone 하면 `.uasset` 이 100바이트짜리 포인터 텍스트로
내려와 프로젝트 전체가 깨져 보입니다. 에디터가 에셋을 못 읽고 원인을 찾느라
몇 시간을 날리는, 가장 흔한 초기 사고입니다.

이미 잘못 받았다면 `git lfs install` 후 `git lfs pull` 로 복구됩니다.

### 3단계 — clone 및 계정 설정

```bash
git clone https://github.com/2nd-run-team/2ND_Run.git
cd 2ND_Run

git config user.name  "본인 이름"
git config user.email "GitHub에 등록된 이메일"   # 중요 — 아래 설명 참고
git config lfs.locksverify true
```

**`user.email` 은 GitHub 계정에 등록된 주소여야 합니다.** 다른 주소로 커밋하면
푸시는 되지만 커밋이 계정과 연결되지 않아 **Contributors 집계에서 빠집니다.**
회색 이름으로만 표시되고, 기여도가 잡히지 않습니다.

이메일을 공개하기 싫으면 GitHub이 제공하는 noreply 주소를 쓰면 됩니다.
Settings → Emails 에서 확인할 수 있는 `12345678+아이디@users.noreply.github.com`
형식이며, 이것도 정상 집계됩니다.

확인:

```bash
git config user.email          # 설정값 확인
git log -1 --format='%ae'      # 실제 커밋에 박힌 주소
```

나중에 이메일을 등록하면 과거 커밋도 소급 반영됩니다.

### 4단계 — 프로젝트 실행

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

## PR 과 코드 리뷰

`main`·`develop` 은 보호되어 있어 직접 푸시가 불가능합니다. 모든 변경은
브랜치 → PR → 리뷰 승인 → Squash merge 경로를 지납니다.

**팀원 누구나 리뷰하고 승인할 수 있습니다.** 팀장 승인을 기다릴 필요 없습니다.

| 조건 | 내용 |
|------|------|
| 승인 | **1명 이상** |
| 승인 자격 | **팀원 누구나** (경로 제한 없음) |
| 상태 검사 | `hygiene` 통과 |
| 머지 방식 | Squash merge, 머지 후 브랜치 자동 삭제 |

[CODEOWNERS](.github/CODEOWNERS) 는 **리뷰 요청을 누구에게 보낼지**만 정합니다.
`Source/` 를 건드린 PR 은 `programmers` 팀에게, `Content/Audio/` 는 `sound`
팀에게 자동으로 요청이 갑니다. 다만 승인 자격은 제한하지 않아서, 담당자가
자리를 비워도 다른 팀원이 승인하면 머지할 수 있습니다.

주의할 점 두 가지:

- **자기 PR 은 자기가 승인할 수 없습니다.** 반드시 다른 사람이 봐야 합니다.
- 리뷰 후 새 커밋을 올리면 기존 승인이 무효화됩니다 (`dismiss_stale_reviews`).
  리뷰어에게 다시 요청하세요.

리뷰할 때 봐야 할 것:

- C++ — 컴파일되는가, 네이밍·구조가 기존 코드와 맞는가
- 에셋 — 에디터에서 실제로 열어봤는가, LFS 락이 풀렸는가
- 공통 — `Saved/`·`Intermediate/` 같은 생성 폴더가 섞여 들어오지 않았는가

PR 템플릿에 체크리스트가 있으니 그대로 따라가면 됩니다.

## 팀

이 저장소는 [`2nd-run-team`](https://github.com/2nd-run-team) 조직 소유입니다.
개인 계정이 아니라 조직에 귀속되어 있어, 구성원이 바뀌어도 소유권 이전이
필요 없습니다.

| 팀 | 권한 | 담당 경로 | 멤버 |
|----|------|----------|------|
| `@2nd-run-team/programmers` | Write | `Source/`, `Config/` | Hoona-Kim, bizet12 |
| `@2nd-run-team/artists` | Write | `Content/` | choisney, bizet12 |
| `@2nd-run-team/sound` | Write | `Content/Audio,Sound,SFX,Music/` | Enginemaster |

**전원 Write 권한으로 동일합니다.** 브랜치 푸시, PR 생성, 리뷰·승인, 이슈 관리,
LFS 락까지 할 수 있는 일에 차이가 없습니다. `main`/`develop` 직접 푸시만
보호 규칙이 막습니다.

팀 구분은 **리뷰 요청 자동 배정** 용도입니다. 권한 차이가 아닙니다.

사운드 에셋은 `Content/` 아래 위 네 폴더 중 하나에 넣으면 `sound` 팀에 리뷰가
갑니다. 다른 이름을 쓰기로 했다면 [CODEOWNERS](.github/CODEOWNERS) 를 수정하세요.

## 라이선스

[MIT](LICENSE)

주의: MIT는 이 저장소의 **코드**에 적용됩니다. 마켓플레이스 에셋, 외부에서
받아온 모델·사운드·폰트 등은 각자의 원 라이선스를 그대로 따르며 MIT로
재배포되지 않습니다. 재배포 제한이 있는 에셋은 커밋 전에 확인하세요.
