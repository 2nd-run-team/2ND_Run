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

## 7. Discord 연동

### 7.1 PR · 푸시 알림 (코드 불필요)

Discord 가 GitHub 웹훅 포맷을 그대로 이해합니다. 설정만 하면 됩니다.

1. Discord 채널 → 톱니바퀴 → 연동 → **웹후크** → 새 웹후크 → URL 복사
2. 복사한 URL 끝에 **`/github` 를 붙입니다**

   ```
   https://discord.com/api/webhooks/1234.../abcd        ← 복사한 것
   https://discord.com/api/webhooks/1234.../abcd/github  ← 등록할 것
   ```

3. 저장소 → Settings → Webhooks → **Add webhook**
   - Payload URL: 위의 `/github` 붙인 주소
   - Content type: `application/json`
   - Events: **Let me select individual events** → `Pull requests`, `Pushes`, `Issues` 정도만 체크
4. Add webhook

`/github` 를 빼면 Discord 가 내용을 해석하지 못해 아무것도 뜨지 않습니다.

### 7.2 잠긴 에셋 목록 자동 게시

[`.github/workflows/lfs-locks-report.yml`](.github/workflows/lfs-locks-report.yml)
이 매일 저녁 7시(KST)에 잠긴 파일 목록을 Discord 에 올립니다.

누가 락을 잡은 채로 퇴근했을 때 다음 사람이 확인하는 용도입니다. 작업 직전
확인은 에디터의 자물쇠 아이콘이 더 빠릅니다.

설정:

1. 7.1 에서 만든 웹후크 URL (이번에는 **`/github` 없이**) 또는 별도 채널의 웹후크
2. 저장소 → Settings → Secrets and variables → Actions → **New repository secret**
   - Name: `DISCORD_WEBHOOK`
   - Secret: 웹후크 URL
3. Actions 탭 → LFS Lock Report → **Run workflow** 로 테스트

동작:

| 상황 | 결과 |
|------|------|
| 예정 실행 · 락 있음 | 목록 게시 |
| 예정 실행 · 락 없음 | 게시 안 함 (채널 소음 방지) |
| 수동 실행 | 락이 없어도 응답 |

시간을 바꾸려면 워크플로의 `cron: '0 10 * * *'` 를 수정하세요. UTC 기준이라
KST 에서 9를 빼면 됩니다 (19시 → 10).
