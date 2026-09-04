# Copy-paste prompt — B5 Native ASIO Control Panel

Use this in a fresh ChatGPT tab.

---

Sound Blaster X4 Windows ARM64 / ARM64EC ASIO 작업을 이어간다.

GitHub 저장소:

`npark2860-cyber/SoundBlaster-X4-ARM64`

이전 대화를 추측해서 복원하지 말고 GitHub를 source of truth로 사용해라.

이번 탭의 작업 범위는 **B5 네이티브 ASIO 제어창(control panel)** 이다. CTCDC/CTIntrfu와 별도 앱 UI 작업은 하지 않는다.

작업 시작 전에 반드시 다음 문서를 전부 읽어라.

- `CURRENT_HANDOFF.md`
- `NEXT_ACTION_ASIO_CONTROL_PANEL.md`
- `NEXT_ACTION_ASIO.md`
- `DEBUG_HISTORY_20260904_ASIO_B5_POST_COALESCE_STALE_WAKE.md`
- 필요 시 `DEBUG_HISTORY_20260904_ASIO_B5_MUX_V4_STATS_ALIAS_REGRESSION.md`

그리고 실제 GitHub의 현재 `main` HEAD와 B5 관련 branch HEAD를 먼저 확인해서 문서와 일치하는지 보고해라.

중요한 상태 구분:

- 현재 B5 productization branch 기준 문서상 HEAD는
  `4475fc17b70f372fe317fa201f201e8dc5543f9f`
- 최신 runtime-code commit은
  `64e34b48714789ab17fba57be34b054f2170b4e9`
- 이 최신 `post-coalesce-stale-v1` 빌드는 **이미 빌드 + product validation 완료** 상태다.
- 최신 product report는 `B5 INSTALL + PRODUCT VALIDATION: PASS`이며,
  48k/240 output/duplex, 96k/240 duplex, REAPER-matched 48k/480 5초,
  192k/384 short cycles, 48k/96, 48k/4800, 48k/512, ASIO capability probe가 모두 PASS했다.
- `post-coalesce-stale-v1`도 dedicated 192k cadence에서 실제 동작이 확인됐다.
  432/480/576에서는 `+2 coalesce -> optional same-packet stale wake`를 여러 번 복구하고도 stop=0으로 끝났다.
- 그러나 192k/384 long cadence에서는 별도의 strict failure가 남아 있다:
  `previous=1375 expected=1376 current=1378 delta=3`
- 이 `delta=3` 문제는 **제어창 작업 범위가 아니다.** panel 작업 중 strict runtime check를 약하게 하거나 WaveRT/mux를 수정하지 마라.
- 192k geometry는 다시 확인해도 동일하다: 384가 최초 할당 가능, 432..960 accepted. geometry probe를 또 시키지 마라.

실제 audible REAPER 기준은 따로 구분해라:

- 이전 `ca37f0e8427227733cd6082a50e20101312e3333` 빌드는 사용자가 ordinary REAPER 재생에서 문제가 없었다고 명시적으로 확인했다.
- 최신 `4475fc...` 빌드는 product matrix와 48k/480 silent REAPER-matched case까지 PASS했지만, 사용자가 최신 빌드로 다시 audible REAPER를 했다고 명시하지 않는 한 이를 real-audio evidence로 과장하지 마라.

제어창 작업은 runtime worker 실험과 분리한다.

권장 분리 branch:

`exp/windows-arm64-asio-b5-control-panel`

branch를 만들기 전에 GitHub에서 refs를 확인하고, 어떤 commit을 base로 쓸지 먼저 명확히 보고해라.

현재 권장 base는 최신 product-matrix validated B5 HEAD:

`4475fc17b70f372fe317fa201f201e8dc5543f9f`

단, 이 branch에서 WaveRT/mux/runtime 파일은 freeze하고 제어창 때문에 수정하지 마라. 192k/384 delta=3 runtime 문제는 별도 작업으로 남긴다.

현재 `driver_b5.cpp`에는:

`ASIOError controlPanel() override { return ASE_NotPresent; }`

가 있다.

목표는 Creative 바이너리를 재사용하지 않는 **자체 네이티브 Win32 ASIO 제어창**을 만드는 것이다.

필수 요구사항:

- ARM64EC + Classic ARM64 둘 다 빌드 가능
- 외부 UI runtime 없이 native Win32 우선
- `IASIO::controlPanel()`에서 열림
- 제품명 `Sound Blaster X4 ARM64 ASIO B5`
- 현재/effective sample rate 표시
- 현재/effective buffer 표시
- buffer/latency를 frames + ms로 표시
- 48/96k: min96 max4800 preferred240 granularity48
- 192k: min384 max4800 preferred384 granularity48
- 512 compatibility 지원
- REAPER가 현재 480 samples를 쓰고 있다면 240을 current라고 표시하지 말고 실제 480을 current/effective로 표시
- panel을 여는 것만으로 WaveRT pin을 생성하면 안 됨
- panel open 시 공격적인 hardware probe 금지
- buffers/RUN 활성 중 geometry를 live mutation 금지
- Apply / OK / Cancel 동작을 명확하고 결정적으로 구현
- 사용자 buffer preference는 다음 안전한 reopen 경로에 적용되도록 persistence
- host가 `createBuffers(bufferSize)`로 실제 buffer size를 넘긴다는 ASIO semantics를 무시하고 panel 값으로 강제 덮어쓰지 말 것
- `kAsioResetRequest`를 쓸 경우 callback lifetime/reentrancy를 먼저 확인하고 근거 없이 바로 넣지 말 것
- diagnostics 기능은 유지. 단 callback마다 파일 쓰기 금지
- 기존 `%TEMP%\B5_RUNTIME_FAILURE.txt` failure-only logging은 유지

UI는 디버그 툴처럼 보이지 않고 compact하고 그럴듯한 오디오 드라이버 설정창이어야 한다.

제어창 작업 때문에 다음을 임의로 수정하지 마라:

- B4D
- BUSY/local/global ownership gate
- WaveRT render/capture core
- mux worker timing/recovery logic
- `post-coalesce-stale-v1`
- joined-worker-before-hardware-teardown
- packet discontinuity / position / copy / callback-index strict checks
- `runtime-failsafe-v1`

요청 범위 외 cleanup/refactor 금지.

처음에는 코드를 바로 바꾸지 말고:

1. GitHub refs 확인
2. 지정 문서 전부 읽기
3. 현재 B5 HEAD 기준 `driver_b5.cpp`, ARM64EC/Classic build adapter, CMake 구조 확인
4. control panel 구현에 필요한 최소 파일 목록과 branch/base 계획 보고
5. `sysHandle`, `g_module`, `callbacks_`, `createBuffers`, `getBufferSize`, `getLatencies`, `setSampleRate`, `disposeBuffers`의 현재 semantics를 짧게 정리
6. WaveRT/mux/runtime 파일을 UI branch에서 건드리지 않는다는 계획을 명시
7. 그 다음 최소 구현으로 진행

사용자의 시간을 보호해야 하므로 이미 끝난 geometry probe, 반복 cadence probe, 반복 REAPER 테스트를 제어창 작업 시작 조건으로 다시 요구하지 마라.
