# 팀 협업 규칙

## 0. 최초 1회 세팅

```bash
git lfs install
git clone <repo-url>
cd <repo>
git config core.autocrlf true          # Windows
git config lfs.locksverify true        # 락 검증 활성화
```

> `git lfs install` 을 하지 않고 clone 하면 `.uasset` 이 120바이트짜리 포인터 텍스트로
> 내려옵니다. 에디터에서 에셋이 깨져 보이면 이것부터 확인하세요.

## 0.5. 엔진 버전 — 5.8.2 고정

전원이 **Unreal Engine 5.8.2**를 씁니다. 패치 번호까지 맞춰야 합니다.

에셋 파일에는 저장한 엔진 버전이 기록됩니다. 상위 버전에서 저장된 에셋은
하위 버전 에디터가 거부합니다. 5.8.2로 저장한 레벨을 5.8.0 쓰는 팀원이 열면
로드에 실패하고, 파일을 되돌릴 방법은 없습니다.

```bash
grep EngineAssociation *.uproject   # 프로젝트가 묶인 엔진 확인
```

업그레이드가 필요하면:

1. 팀 채널에 제안하고 합의
2. **전원 동시에** 에픽 런처에서 업데이트
3. 한 명이 프로젝트를 열어 컨버전한 뒤 PR (README 배지·표도 함께 수정)
4. 나머지는 그 PR 머지 후 pull

절대 혼자 올리지 마세요. 되돌릴 수 없습니다.

## 1. 브랜치 전략

| 브랜치 | 용도 | 직접 푸시 |
|--------|------|-----------|
| `main` | 언제나 빌드 가능한 안정 버전 | ❌ (PR only) |
| `develop` | 통합 개발 브랜치 | ❌ (PR only) |
| `feature/<이름>-<작업>` | 기능 개발 | ✅ |
| `fix/<이름>-<버그>` | 버그 수정 | ✅ |

예: `feature/kw-player-dash`, `fix/kw-nav-crash`

## 2. 에셋 작업 전 반드시 락 (가장 중요)

`.uasset` / `.umap` 은 **머지가 불가능**합니다. 두 사람이 동시에 수정하면
한쪽 작업은 무조건 버려집니다. 그래서 편집 전에 잠급니다.

```bash
# 잠그기 (작업 시작 전)
git lfs lock Content/Maps/MainLevel.umap

# 누가 뭘 잠갔는지 확인
git lfs locks

# 풀기 (푸시 완료 후)
git lfs unlock Content/Maps/MainLevel.umap
```

규칙:
- **레벨(`.umap`) 작업은 반드시 락을 먼저 잡는다.** 예외 없음.
- 락은 당일 안에 푼다. 오래 잡아야 하면 팀 채널에 공유.
- 남의 락을 `--force` 로 푸는 건 본인 동의 후에만.

## 3. 커밋 메시지

```
<type>: <요약>

<본문 — 왜 바꿨는지>
```

타입: `feat`, `fix`, `refactor`, `art`, `level`, `docs`, `chore`, `ci`

예)
```
feat: 플레이어 대시 어빌리티 추가
art: 주인공 머티리얼 러프니스 조정
level: MainLevel 스폰 포인트 재배치
```

## 4. PR 규칙

- 리뷰어 최소 1명 승인 후 머지
- C++ 변경 → 컴파일 확인 필수
- 에셋 변경 → 에디터에서 실제로 열어보고 확인
- 머지 방식: **Squash and merge**

## 5. 하면 안 되는 것

- ❌ `Saved/`, `Intermediate/`, `Binaries/`, `DerivedDataCache/` 커밋
- ❌ 락 없이 `.umap` 수정
- ❌ `main` 에 직접 푸시
- ❌ `Config/*.ini` 를 상의 없이 대폭 수정 (전원에게 영향)
- ❌ `git push --force` (공용 브랜치)

## 6. 충돌이 났을 때

에셋 충돌은 자동 머지가 불가능하므로 **둘 중 하나를 고르는 것**이 유일한 해결책입니다.

```bash
# 내 버전 유지
git checkout --ours Content/...
# 상대 버전 유지
git checkout --theirs Content/...
git add Content/...
```

그 전에 반드시 상대방과 이야기하세요. 한쪽 작업이 사라집니다.
