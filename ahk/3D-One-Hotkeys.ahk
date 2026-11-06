#Requires AutoHotkey v2.0
#SingleInstance Force

; ============================================================================
;  3D-One 통합 단축키 스크립트
;  Setup-3DOne.ps1 이 생성함 — 직접 편집하지 말 것 (재실행 시 덮어써진다)
;
;  AHK 는 스크립트 1개당 프로세스 1개, 프로세스 1개당 트레이 아이콘 1개다.
;  AstraDepth3D 용과 한/영 용을 한 파일로 합쳐 아이콘을 1개로 유지한다.
; ============================================================================

; --------------------------------------------------------- [AstraDepth3D]
; ============================================================================
;  AstraDepth3D 단축키 보정 스크립트
;  자세한 배경은 같은 폴더의 README.md 참고
; ============================================================================

; ----------------------------------------------------------------------------
;  [활성] Ctrl+F4 로 앱이 종료되는 것 방지
; ----------------------------------------------------------------------------
;  AstraDepth3D 는 Ctrl+F4 를 RegisterHotKey 로 전역 종료 단축키로 등록한다.
;  저수준 키보드 훅은 전역 핫키 처리보다 먼저 실행되므로, 여기서 삼키면
;  앱의 종료 핫키가 발동할 기회 자체가 없어진다.
;
;  범위 주의:
;    WinActive("ahk_exe AstraDepth3D.exe") 로는 막히지 않는다.
;    이 앱은 트레이 상주형이라 포커스를 갖는 일이 없기 때문이다.
;    그래서 ProcessExist 로 "앱이 살아 있는 동안" 전역으로 삼킨다.
;
;  부작용:
;    앱 실행 중에는 다른 프로그램에서도 Ctrl+F4 가 동작하지 않는다.
;    회피 불가능하다 — 막지 않으면 앱이 죽고, 막으면 아무도 받지 못한다.
;
;  권한:
;    앱이 requireAdministrator(높은 무결성)라서, 이 스크립트도 관리자 권한으로
;    실행되어야 훅이 유효하다(UIPI). 작업 스케줄러 RunLevel=Highest 로 해결.
;
;  2026-08-05 수정 — 차단이 새던 문제:
;    이전에는 #HotIf ProcessExist("AstraDepth3D.exe") 를 직접 썼다.
;    #HotIf 조건식은 AHK 메인 스레드에서 평가되는데, 평가가 늦어지면 AHK 는
;    조건을 "불일치"로 처리하고 키를 그대로 통과시킨다. ProcessExist() 는
;    프로세스 목록 전체를 훑는 비싼 호출이라, 게임 중 부하가 걸리거나 메인
;    스레드가 다른 핫키를 처리 중이면 제때 끝나지 않는다.
;    → 1초마다 갱신하는 전역 변수로 캐시해 조건 평가를 즉시 끝나게 바꿨다.

;  2026-08-05 2차 수정 — 훅만으로는 부족했다:
;    조건식을 캐시로 바꾼 뒤에도 게임 중 종료가 발생했다. 로그 대조 결과
;    14회 중 13회는 막혔고 1회가 샜다(앱 종료 시각 18:37:29.93,
;    차단 기록 18:37:30). 원인은 Windows 의 LowLevelHooksTimeout —
;    훅이 제한 시간(기본 300ms) 안에 응답하지 못하면 Windows 가 그 키에
;    대해 훅을 건너뛰고 그대로 전달한다. 게임 부하에서 간헐적으로 발생하며,
;    훅 기반 차단은 원리상 이 문제를 피할 수 없다.
;
;    → RegisterHotKey 선점으로 전환. 먼저 등록한 쪽이 독점하므로,
;      AHK 가 앱보다 먼저 Ctrl+F4 를 잡으면 앱의 등록이 실패해 종료
;      단축키 자체가 생기지 않는다. 부하와 무관하게 확정적으로 동작한다.
;      훅 차단은 2차 방어선으로 남겨둔다.

; AHK 스레드가 게임에 밀리지 않도록 우선순위를 올린다(훅 응답 지연 완화).
try ProcessSetPriority "High"

; --- Ctrl+F4 전역 선점 ---
; hwnd=0 으로 등록하면 WM_HOTKEY 가 이 스레드 큐로 오고 AHK 는 모르는
; 메시지라 무시한다. 결과적으로 키가 아무 동작도 일으키지 않는다.
; MOD_NOREPEAT 유무로 별개 취급될 가능성에 대비해 두 변형 모두 시도한다.
PreemptCtrlF4() {
    global g_AstraBlockLog
    ok1 := DllCall("RegisterHotKey", "Ptr", 0, "Int", 0xF41, "UInt", 0x0002, "UInt", 0x73)
    e1  := A_LastError
    ok2 := DllCall("RegisterHotKey", "Ptr", 0, "Int", 0xF42, "UInt", 0x4002, "UInt", 0x73)
    e2  := A_LastError
    try FileAppend(FormatTime(, "yyyy-MM-dd HH:mm:ss")
                 . "  [선점] plain=" ok1 "(err " e1 ")  norepeat=" ok2 "(err " e2 ")`n"
                 , g_AstraBlockLog)
}

; 앱 실행 여부 캐시 (조건식을 변수 읽기 한 번으로 만들기 위함)
g_AstraRunning := false
g_AstraBlockLog := EnvGet("LOCALAPPDATA") . "\3D-One-Hotkeys\ctrl-f4-blocked.log"
PreemptCtrlF4()

UpdateAstraRunning() {
    global g_AstraRunning
    g_AstraRunning := ProcessExist("AstraDepth3D.exe") ? true : false
}
UpdateAstraRunning()
SetTimer UpdateAstraRunning, 1000

#HotIf g_AstraRunning
^F4::
{
    global g_AstraBlockLog
    ; 차단 기록 — 실제로 막혔는지 나중에 확인할 수 있게 남긴다.
    ; 불필요하면 아래 두 줄을 지우고 return 만 남겨도 된다.
    try FileAppend(FormatTime(, "yyyy-MM-dd HH:mm:ss") . "  Ctrl+F4 차단됨`n", g_AstraBlockLog)
    return
}
#HotIf


; ----------------------------------------------------------------------------
;  [비활성] 단축키 리맵 — 필요하면 /* */ 를 벗겨서 사용
; ----------------------------------------------------------------------------
;  앱이 자체 등록하는 전역 단축키
;    Ctrl + ↑   3D 교직(interweave) On/Off 토글   ← 로그로 확인됨
;    Ctrl + T   (미확인. SBS↔AI 변환 타입 전환으로 추정)
;
;  주의 1: Shift+T 는 "대문자 T" 입력이다. 전역(ProcessExist)으로 두면
;          어디서든 대문자 T 를 칠 때 글자가 안 찍히고 3D 기능이 실행된다.
;  주의 2: Ctrl+T 를 브라우저 새 탭으로 되돌리는 것은 불가능하다.
;          앱이 RegisterHotKey 로 전역 선점하므로, 막을 수는 있어도
;          포그라운드 앱에 넘겨줄 수는 없다.
;  주의 3: Send 로 Ctrl+↑ 를 합성해 앱의 전역 핫키를 유발하는 방식이다.
;          RegisterHotKey 는 수식키가 정확히 일치해야 발동하므로, AHK 의 Send 가
;          눌려 있는 Shift 를 임시로 떼었다 복원해 준다.

/*
; --- 권장안: Ctrl+Shift+↑ / Ctrl+Shift+T (전역, 타이핑 충돌 없음) ---
#HotIf ProcessExist("AstraDepth3D.exe")
^+Up:: Send("^{Up}")
^+t::  Send("^t")
#HotIf
*/

/*
; --- 원안: Shift+↑ / Shift+T 를 앱 창이 활성일 때만 (타이핑 안전하지만
;     트레이 상주형이라 실제로는 거의 발동하지 않는다) ---
#HotIf WinActive("ahk_exe AstraDepth3D.exe")
+Up:: Send("^{Up}")
+t::  Send("^t")
#HotIf
*/

/*
; --- 원래 조합 무효화 (위 리맵을 켠 뒤 기존 조합을 막고 싶을 때) ---
; 주의: Ctrl+T 를 막아도 브라우저로 넘어가지 않는다. 아무도 못 받게 될 뿐이다.
#HotIf ProcessExist("AstraDepth3D.exe")
^Up:: return
^t::  return
#HotIf
*/

; ----------------------------------------------------------- [한/영 전환]
;  RAlt 를 한/영(vk15)으로 변환.
;  관리자 권한 앱(관리자 VS Code 등)과 Moonlight/Artemis 스트리밍에서도
;  동작한다. 한/영키 스캔코드는 Moonlight 프로토콜을 통과하지 못하지만,
;  RAlt 는 평범한 키라서 호스트까지 전달된 뒤 여기서 변환된다.

RAlt::Send "{vk15}"

; Alt + Shift 로 Alt + Tab
LAlt & Shift::Send "{Alt down}{Tab}"

LAlt up::Send "{Alt up}"

