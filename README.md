# 2ND_Run

![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.8.2-0E1128?logo=unrealengine)

Unreal Engine **5.8.2** 블루프린트 프로젝트. 실행 파일은 `SecondRun.uproject`
입니다. (UE 프로젝트명은 숫자로 시작할 수 없어 저장소명과 다릅니다.)

## 요구 사항

| 항목 | 버전 |
|------|------|
| Unreal Engine | **5.8.2 고정** |
| Git LFS | 3.x 이상 · [설치](https://git-lfs.com) |

> **엔진 버전은 전원이 5.8.2로 맞춥니다.** 상위 버전으로 저장한 에셋은 하위
> 버전에서 열리지 않고 되돌릴 방법이 없습니다. 업그레이드는 팀 전체가 동시에
> 합니다 — 절차는 [CONTRIBUTING.md](CONTRIBUTING.md) 참고.

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

**4. 실행** — `SecondRun.uproject` 더블클릭

블루프린트 프로젝트라 빌드 과정이 없습니다. 단, 첫 실행은 셰이더 컴파일로
10~30분 걸립니다. 중간에 끄면 다음에 처음부터 다시 합니다.

**5. 에디터에서 소스 컨트롤 연결** (에셋 락 기능)

Plugins → `Git LFS 2 Source Control` 활성화 → 재시작 → 우하단 Source Control
→ Provider `Git (beta version)` → **Use Git LFS file locking workflow** 체크

## 폴더 구조

```
Content/      에셋 — LFS 관리, 수정 전 락 필요
Config/       프로젝트 설정 — 변경 시 팀 공유
```

`Intermediate/`, `Saved/`, `DerivedDataCache/` 는 로컬에서 생성되는 캐시라
저장소에 올리지 않습니다.

## 작업 규칙

`main` · `develop` 직접 푸시는 막혀 있습니다. 브랜치 → PR → Squash merge.

```bash
git checkout develop && git pull
git checkout -b feature/이름-작업
# 작업 후
git push -u origin feature/이름-작업
gh pr create --base develop --fill
```

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

자세한 내용은 [CONTRIBUTING.md](CONTRIBUTING.md) 를 보세요.

## 문제 해결

| 증상 | 원인 |
|------|------|
| 에셋이 전부 깨져 보임 | `git lfs install` 누락 → 설치 후 `git lfs pull` |
| 푸시 거부 | 초대 미수락, 또는 `main`/`develop` 직접 푸시 시도 |
| 파일이 읽기 전용이라 저장 불가 | 정상 동작. 락을 잡아야 합니다 |
| `unable to unlink ... Invalid argument` | 위와 같은 원인 |
| 프로젝트가 안 열림 | 엔진 버전 불일치 → 5.8.2 확인 |
