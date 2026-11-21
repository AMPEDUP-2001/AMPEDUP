from pathlib import Path
import argparse, hashlib
p=argparse.ArgumentParser(); p.add_argument('--guest',type=Path,required=True); p.add_argument('--overlay',type=Path,required=True); p.add_argument('--out',type=Path,required=True); a=p.parse_args()
g=a.guest.read_text(); o=a.overlay.read_text(); start='void sub_0002D200(void)\n{'; assert g.count(start)==1
idx=g.index(start); end=g.index('\n}\n',idx)+3; old=g[idx:end]
assert hashlib.sha256(old.encode()).hexdigest()=='b3dbd4d416e96b7e30d99a6e906b63b691d3e056f181fa803d5e874bc2a1ef10'
new='''extern void amped_demo_unavailable_notice(void);
#if defined(AMPED_UNIFIED_EXPERIMENT)
extern void amped_title_menu_command(unsigned);
#endif
void sub_0002D200(void)
{
#if defined(AMPED_UNIFIED_EXPERIMENT)
    /* Lua 4.1 alpha: the state holds top and CallInfo. The call frame
       holds base at +4. Each argument occupies 8 bytes and a tag. */
    uint32_t state = MEM32(esp + 4);
    uint32_t base = MEM32(MEM32(state + 4) + 4);
    uint32_t top = MEM32(state);
    if (top == base + 9 && MEM8(base + 8) == 2) {
        uint64_t bits = (uint64_t)MEM32(base + 4) << 32 | MEM32(base);
        for (unsigned command = 1; command <= 11; ++command) {
            union { double number; uint64_t bits; } value = { (double)command };
            if (bits == value.bits) {
                amped_title_menu_command(command);
                eax = 0;
                esp += 4;
                return;
            }
        }
    }
#endif
    amped_demo_unavailable_notice();
    eax = 0;
    esp += 4; return;
}
'''
g=g[:idx]+new+g[end:]
anchor='int xbox_OverlayWants(void)\n{'; assert o.count(anchor)==1
o=o.replace(anchor,'static std::atomic<unsigned> s_amp_demo_notice{0};\nextern "C" void amped_demo_unavailable_notice(void) { if (getenv("AMPED_DEMO_NOTICE_TRACE")) (fprintf)(stderr, "[AMPED_DEMO_NOTICE] requested\\n"); s_amp_demo_notice.store(1, std::memory_order_release); }\n'+anchor)
old_wants='|| notice_on() || scripted_on() || sg_banner_on() || touch_on());'; assert o.count(old_wants)==1
o=o.replace(old_wants,'|| notice_on() || scripted_on() || sg_banner_on() || touch_on() || s_amp_demo_notice.load(std::memory_order_acquire) != 0);')
old='    bool note = notice_on(), scripted = scripted_on(), sg = sg_banner_on();\n'; assert o.count(old)==1
new='    static uint32_t amp_demo_start = 0;\n    static bool amp_demo_show = false;\n    unsigned amp_demo_pending = 1;\n    if (s_amp_demo_notice.compare_exchange_strong(amp_demo_pending, 2, std::memory_order_acq_rel)) { amp_demo_start = now; amp_demo_show = true; if (getenv("AMPED_DEMO_NOTICE_TRACE")) (fprintf)(stderr, "[AMPED_DEMO_NOTICE] consumed at=%u\\n", now); }\n    if (amp_demo_show && (uint32_t)(now - amp_demo_start) >= 4000u) { amp_demo_show = false; unsigned active = 2; s_amp_demo_notice.compare_exchange_strong(active, 0, std::memory_order_acq_rel); if (getenv("AMPED_DEMO_NOTICE_TRACE")) (fprintf)(stderr, "[AMPED_DEMO_NOTICE] expired at=%u\\n", now); }\n'+old
o=o.replace(old,new)
old='if (!s_open && s_alpha <= 0.0f && !perf && !note && !scripted && !sg && !touch && !title_row)'; assert o.count(old)==1
o=o.replace(old,old[:-1]+' && !amp_demo_show)')
anchor='    if (scripted) {\n        /* Top centre, out of the HUD\'s way. */'; assert o.count(anchor)==1
block='''    if (amp_demo_show) {
        const char *text = "Game Demos is unavailable in this version";
        ImDrawList *dl = ImGui::GetForegroundDrawList();
        const ImVec2 disp = ImGui::GetIO().DisplaySize;
        const float tw = ImGui::CalcTextSize(text).x;
        const ImVec2 p((disp.x - tw) * 0.5f, disp.y * 0.045f);
        dl->AddRectFilled(ImVec2(p.x - fs, p.y - fs * 0.5f), ImVec2(p.x + tw + fs, p.y + fs * 1.5f), solid(T->toast_bg, 235 / 255.0f), fs * 0.5f);
        dl->AddText(p, solid(T->toast_text), text);
    }
'''
o=o.replace(anchor,block+anchor)
a.out.mkdir(parents=True,exist_ok=True); (a.out/'recomp_0007.c').write_text(g,newline='\n'); (a.out/'overlay_ui.cpp').write_text(o,newline='\n')
print('validated exact callback and unique overlay anchors')
