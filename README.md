# 2ND_Run

![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.8.2-0E1128?logo=unrealengine)
![Language](https://img.shields.io/badge/C%2B%2B-project-00599C?logo=cplusplus)

Unreal Engine **5.8.2** C++ 프로젝트. 실행 파일은 `SecondRun.uproject`
입니다. (UE 프로젝트명은 숫자로 시작할 수 없어 저장소명과 다릅니다.)

## 요구 사항

| 항목 | 버전 |
|------|------|
| Unreal Engine | **5.8.2 고정** |
| Visual Studio | **2022** · `C++를 사용한 게임 개발` 워크로드 필수 |
| Git LFS | 3.x 이상 · [설치](https://git-lfs.com) |

> **엔진 버전은 전원이 5.8.2로 맞춥니다.** 상위 버전으로 저장한 에셋은 하위
> 버전에서 열리지 않고 되돌릴 방법이 없습니다. 업그레이드는 팀 전체가 동시에
> 합니다 — 절차는 [CONTRIBUTING.md](CONTRIBUTING.md) 참고.

> **Visual Studio 는 선택이 아닙니다.** C++ 프로젝트라 컴파일 없이는 에디터가
> 열리지 않습니다. 설치 시 워크로드 목록에서 `C++를 사용한 게임 개발` 을
> 반드시 체크하세요. 이미 VS 가 있다면 Visual Studio Installer → 수정 →
> 해당 워크로드 추가.

## 시작하기

**1. 조직 초대 수락** — https://github.com/orgs/2nd-run-team/invitations
(7일 뒤 만료. 수락 전에는 푸시 불가)

**2. clone 전에 LFS 설치** ← 가장 흔한 사고 지점

```bash
git lfs install
```

안 하면 `.uasset` 이 포인터 텍스트로 내려와 프로젝트가 전부 깨져 보입니다.
이미 그렇게 받았다면 `git lfs install` 후 `git lfs pull` 로 복구됩니다.

**3. clone 및 설정**

```bash
git clone https://github.com/2nd-run-team/2ND_Run.git
cd 2ND_Run
git config user.name  "본인 이름"
git config user.email "GitHub에 등록된 이메일"
git config lfs.locksverify true
```

저장 위치는 **OneDrive 바깥**으로 하세요. 동기화 충돌로 빌드가 깨집니다.
`user.email` 이 GitHub 등록 주소가 아니면 커밋이 계정에 연결되지 않아
Contributors 집계에서 빠집니다.

**4. 빌드** ← 블루프린트 프로젝트와 다른 점

C++ 프로젝트라 clone 직후에는 컴파일이 필요합니다. `.uproject` 를 바로
더블클릭하면 "모듈을 빌드해야 합니다" 프롬프트가 뜨고, 여기서 실패하면
에디터가 아예 열리지 않습니다.

```
1. SecondRun.uproject 우클릭 → Generate Visual Studio project files
2. 생성된 SecondRun.sln 을 Visual Studio 2022 로 열기
3. 솔루션 구성을 Development Editor / Win64 로 맞추고 빌드 (Ctrl+Shift+B)
```

빌드가 끝나면 이후로는 `.uproject` 더블클릭으로 바로 열립니다.
단, **`Source/` 가 바뀐 브랜치를 pull 받으면 다시 빌드해야 합니다.**

`Generate Visual Studio project files` 항목이 우클릭 메뉴에 없으면 엔진
연결이 끊긴 것입니다. 에픽 런처에서 UE 5.8.2 를 한 번 실행해 보세요.

**5. 첫 실행** — 셰이더 컴파일로 10~30분 걸립니다. 중간에 끄면 다음에
처음부터 다시 합니다.

**6. 에디터에서 소스 컨트롤 연결** (에셋 락 기능)

`Plugins/GitSourceControl/` 에 포함된 `Git LFS 2` 플러그인은 프로젝트에서 자동 활성화됩니다.
우하단 Source Control → Provider `Git LFS 2` → **Uses Git LFS 2 File Locking workflow** 체크
→ **LFS Username** 에 자신의 GitHub 사용자명 입력. 에셋 작업 전 콘텐츠 브라우저에서
해당 파일을 우클릭해 **Check Out** 으로 락을 잡습니다.

락 기능을 사용하려면 GitHub 인증이 필요합니다. 플러그인의 에디터 내 Pull/Submit 대신
팀 브랜치와 PR 절차를 사용하세요. Pull은 rebase를, Submit은 직접 push를 할 수 있습니다.

## 폴더 구조

```
Source/       C++ 소스 — 머지 가능, 락 불필요
Content/      에셋 — LFS 관리, 수정 전 락 필요
Config/       프로젝트 설정 — 변경 시 팀 공유
```

`Intermediate/`, `Saved/`, `Binaries/`, `DerivedDataCache/`, `*.sln` 은
로컬에서 생성되는 빌드 산출물이라 저장소에 올리지 않습니다.

## 작업 규칙

`main` · `develop` 직접 푸시는 막혀 있습니다. 브랜치 → PR → Squash merge.

```bash
git checkout develop && git pull
git checkout -b feature/player-dash
# 작업 후
git push -u origin feature/player-dash
gh pr create --base develop --fill
```

브랜치 이름은 **`<타입>/<작업>`** — 작성자 이름은 넣지 않습니다.

| 타입 | 담당 | 용도 | 예 |
|------|------|------|-----|
| `feature/` | 프로그래머 | 코드 · 기능 · 버그 수정 · 설정 · 문서 | `feature/player-dash` |
| `art/` | 아티스트 | 에셋 · 머티리얼 · 텍스처 · 레벨 | `art/character-material` |
| `sound/` | 사운드 | BGM · 효과음 · 사운드 큐 | `sound/footstep-sfx` |

영소문자와 하이픈만, 2~4단어로. 자세한 설명은 PR 본문에 씁니다.
브랜치 이름으로는 기능/수정이 구분되지 않지만, 커밋 메시지 타입
(`feat` `fix` `art` …)으로 구분됩니다.

| 항목 | 설정 |
|------|------|
| 승인 | 필수 아님 (리뷰는 권장) |
| 상태 검사 | `hygiene` 통과 필수 |
| 머지 | Squash, 머지 후 브랜치 자동 삭제 |

**에셋 작업 전에는 반드시 락을 잡으세요.** `.uasset` / `.umap` 은 머지가
불가능해서 동시 수정 시 한쪽 작업이 통째로 사라집니다.

```bash
git lfs lock Content/경로/파일.umap
git lfs locks                          # 누가 뭘 잠갔는지
git lfs unlock Content/경로/파일.umap   # 푸시 후 해제
```

에디터에서는 에셋 우클릭 → Check Out 으로 대체됩니다.

**`Source/` 의 `.cpp` / `.h` 는 락이 필요 없습니다.** 일반 텍스트라 git 이
머지할 수 있습니다. 대신 `Source/` 를 건드린 PR 은 머지 후 전원이 리빌드해야
하므로 팀 채널에 알려주세요.

자세한 내용은 [CONTRIBUTING.md](CONTRIBUTING.md) 를 보세요.

## 문제 해결

| 증상 | 원인 |
|------|------|
| 에셋이 전부 깨져 보임 | `git lfs install` 누락 → 설치 후 `git lfs pull` |
| 푸시 거부 | 초대 미수락, 또는 `main`/`develop` 직접 푸시 시도 |
| 파일이 읽기 전용이라 저장 불가 | 정상 동작. 락을 잡아야 합니다 |
| `unable to unlink ... Invalid argument` | 위와 같은 원인 |
| 프로젝트가 안 열림 | 엔진 버전 불일치 → 5.8.2 확인 |
| "모듈을 빌드해야 합니다" 후 실패 | VS 2022 + C++ 게임 개발 워크로드 미설치 |
| pull 후 에디터가 안 열림 / 크래시 | `Source/` 변경됨 → VS 에서 다시 빌드 |
| 빌드는 되는데 새 C++ 클래스가 안 보임 | `Generate Visual Studio project files` 재실행 |
| 빌드가 계속 깨짐 | `Intermediate/`, `Binaries/` 삭제 후 4번 재수행 |
