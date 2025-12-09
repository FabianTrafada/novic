#include <gtkmm.h>
#include <gtk-layer-shell.h>
#include <giomm/dbusproxy.h>
#include <pulse/pulseaudio.h>
#include <iostream>
#include <memory>
#include <sstream>
#include <cmath>
#include <array>

constexpr int NUM_BARS = 7;
constexpr int COLLAPSED_WIDTH = 400;
constexpr int COLLAPSED_HEIGHT = 60;
constexpr int EXPANDED_WIDTH = 620;
constexpr int EXPANDED_HEIGHT = 240;

class AudioVisualizer {
public:
    AudioVisualizer() {
        for (auto& level : levels) level = 0.0f;
        for (auto& level : smoothed_levels) level = 0.0f;
        setup_pulseaudio();
    }
    ~AudioVisualizer() { cleanup_pulseaudio(); }
    std::array<float, NUM_BARS> get_levels() const { return smoothed_levels; }
    void set_playing(bool playing) { is_playing = playing; if (!playing) for (auto& l : levels) l = 0.0f; }
    void update() {
        for (int i = 0; i < NUM_BARS; i++) {
            if (levels[i] > smoothed_levels[i]) smoothed_levels[i] += (levels[i] - smoothed_levels[i]) * 0.4f;
            else smoothed_levels[i] *= 0.88f;
            smoothed_levels[i] = std::clamp(smoothed_levels[i], 0.0f, 1.0f);
            if (smoothed_levels[i] < 0.02f) smoothed_levels[i] = 0.0f;
        }
    }
private:
    pa_mainloop* mainloop = nullptr; pa_mainloop_api* mainloop_api = nullptr;
    pa_context* context = nullptr; pa_stream* stream = nullptr;
    std::array<float, NUM_BARS> levels{}, smoothed_levels{}; bool is_playing = false;
    void setup_pulseaudio() {
        mainloop = pa_mainloop_new(); mainloop_api = pa_mainloop_get_api(mainloop);
        context = pa_context_new(mainloop_api, "Novic");
        pa_context_set_state_callback(context, ctx_cb, this);
        pa_context_connect(context, nullptr, PA_CONTEXT_NOFLAGS, nullptr);
        Glib::signal_timeout().connect([this]() { if (mainloop) pa_mainloop_iterate(mainloop, 0, nullptr); return true; }, 10);
    }
    void cleanup_pulseaudio() {
        if (stream) { pa_stream_disconnect(stream); pa_stream_unref(stream); }
        if (context) { pa_context_disconnect(context); pa_context_unref(context); }
        if (mainloop) pa_mainloop_free(mainloop);
    }
    static void ctx_cb(pa_context* c, void* u) { if (pa_context_get_state(c) == PA_CONTEXT_READY) static_cast<AudioVisualizer*>(u)->setup_stream(); }
    void setup_stream() { auto op = pa_context_get_server_info(context, srv_cb, this); if (op) pa_operation_unref(op); }
    static void srv_cb(pa_context* c, const pa_server_info* i, void* u) {
        auto* s = static_cast<AudioVisualizer*>(u); if (!i || !i->default_sink_name) return;
        pa_sample_spec ss = {PA_SAMPLE_FLOAT32LE, 44100, 2};
        s->stream = pa_stream_new(c, "Novic", &ss, nullptr); if (!s->stream) return;
        pa_stream_set_read_callback(s->stream, read_cb, u);
        pa_buffer_attr a = {(uint32_t)-1,(uint32_t)-1,(uint32_t)-1,(uint32_t)-1,4096};
        pa_stream_connect_record(s->stream, (std::string(i->default_sink_name)+".monitor").c_str(), &a, PA_STREAM_ADJUST_LATENCY);
    }
    static void read_cb(pa_stream* s, size_t len, void* u) {
        auto* self = static_cast<AudioVisualizer*>(u); const void* d;
        if (pa_stream_peek(s, &d, &len) < 0 || !d) { pa_stream_drop(s); return; }
        auto* samp = static_cast<const float*>(d); size_t n = len/sizeof(float);
        for (int i = 0; i < NUM_BARS; i++) {
            size_t off = (i*n/NUM_BARS)%n, bs = n/NUM_BARS; float r = 0;
            for (size_t j = 0; j < bs && off+j < n; j++) r += samp[off+j]*samp[off+j];
            self->levels[i] = std::min(1.0f, std::sqrt(r/bs)*15.0f) * (0.8f + 0.2f*(i%3)/2.0f);
        }
        pa_stream_drop(s);
    }
};

class MediaMonitor {
public:
    struct MediaInfo { std::string title, artist, album, player_name, icon_name, art_url, bus_name; bool is_playing = false; int64_t position = 0, length = 0; };
    MediaMonitor() { check(); Glib::signal_timeout().connect(sigc::mem_fun(*this, &MediaMonitor::check), 500); }
    MediaInfo get_current_media() const { return current; }
    sigc::signal<void(MediaInfo)> signal_media_changed() { return sig; }
    void play_pause() { cmd("PlayPause"); } void next() { cmd("Next"); } void previous() { cmd("Previous"); }
private:
    MediaInfo current; sigc::signal<void(MediaInfo)> sig;
    void cmd(const std::string& c) { if (current.bus_name.empty()) return; try {
        Gio::DBus::Connection::get_sync(Gio::DBus::BUS_TYPE_SESSION)->call_sync("/org/mpris/MediaPlayer2","org.mpris.MediaPlayer2.Player",c,{},current.bus_name);
    } catch(...){} }
    bool check() { try {
        auto conn = Gio::DBus::Connection::get_sync(Gio::DBus::BUS_TYPE_SESSION);
        auto r = conn->call_sync("/org/freedesktop/DBus","org.freedesktop.DBus","ListNames",{},"org.freedesktop.DBus");
        Glib::Variant<std::vector<Glib::ustring>> nv; r.get_child(nv,0);
        std::vector<Glib::ustring> players; for (auto& n : nv.get()) if (n.find("org.mpris.MediaPlayer2.")==0) players.push_back(n);
        MediaInfo play, pause; bool fp=false, hp=false;
        for (auto& p : players) { auto i = get_info(conn,p); if (i.is_playing) {play=i;fp=true;break;} else if (!hp&&!i.title.empty()) {pause=i;hp=true;} }
        auto* sel = fp?&play:(hp?&pause:nullptr);
        if (sel && (current.title!=sel->title||current.is_playing!=sel->is_playing||current.position!=sel->position)) { current=*sel; sig.emit(current); }
        else if (!sel && !current.title.empty()) { current={}; sig.emit(current); }
    } catch(...){} return true; }
    MediaInfo get_info(Glib::RefPtr<Gio::DBus::Connection> c, const Glib::ustring& bus) {
        MediaInfo i; i.bus_name=bus; i.player_name=bus.substr(23); try {
            auto get_prop = [&](const std::string& iface, const std::string& prop) {
                return c->call_sync("/org/mpris/MediaPlayer2","org.freedesktop.DBus.Properties","Get",
                    Glib::VariantContainerBase::create_tuple({Glib::Variant<Glib::ustring>::create(iface),Glib::Variant<Glib::ustring>::create(prop)}),bus);
            };
            Glib::VariantBase v; get_prop("org.mpris.MediaPlayer2.Player","PlaybackStatus").get_child(v,0);
            try { i.is_playing = Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(v).get()=="Playing"; }
            catch(...) { i.is_playing = Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::VariantBase>>(v).get()).get()=="Playing"; }
            try { Glib::VariantBase pv; get_prop("org.mpris.MediaPlayer2.Player","Position").get_child(pv,0);
                try { i.position = Glib::VariantBase::cast_dynamic<Glib::Variant<int64_t>>(pv).get(); }
                catch(...) { i.position = Glib::VariantBase::cast_dynamic<Glib::Variant<int64_t>>(Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::VariantBase>>(pv).get()).get(); }
            } catch(...) {}
            get_prop("org.mpris.MediaPlayer2.Player","Metadata").get_child(v,0);
            i.title="Unknown"; i.artist="Unknown"; i.album=""; i.icon_name="multimedia-player";
            try { auto mi = Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::VariantBase>>(v);
                auto m = Glib::VariantBase::cast_dynamic<Glib::Variant<std::map<Glib::ustring,Glib::VariantBase>>>(mi.get()).get();
                if (m.count("xesam:title")) i.title = Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(m["xesam:title"]).get();
                if (m.count("xesam:album")) i.album = Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(m["xesam:album"]).get();
                if (m.count("xesam:artist")) { try { auto a = Glib::VariantBase::cast_dynamic<Glib::Variant<std::vector<Glib::ustring>>>(m["xesam:artist"]).get(); if(!a.empty()) i.artist=a[0]; } catch(...) { try { i.artist=Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(m["xesam:artist"]).get(); } catch(...){} } }
                if (m.count("mpris:artUrl")) try { i.art_url = Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(m["mpris:artUrl"]).get(); } catch(...){}
                if (m.count("mpris:length")) try { i.length = Glib::VariantBase::cast_dynamic<Glib::Variant<int64_t>>(m["mpris:length"]).get(); } catch(...){}
            } catch(...) {}
            if (i.player_name.find("spotify")!=std::string::npos) i.icon_name="spotify";
        } catch(...) {} return i;
    }
};

class NovicWindow : public Gtk::Window {
public:
    NovicWindow() {
        set_title("Novic"); set_default_size(COLLAPSED_WIDTH, COLLAPSED_HEIGHT);
        set_decorated(false); set_app_paintable(true);
        add_events(Gdk::ENTER_NOTIFY_MASK | Gdk::LEAVE_NOTIFY_MASK);
        gtk_layer_init_for_window(GTK_WINDOW(gobj()));
        gtk_layer_set_layer(GTK_WINDOW(gobj()), GTK_LAYER_SHELL_LAYER_TOP);
        gtk_layer_set_anchor(GTK_WINDOW(gobj()), GTK_LAYER_SHELL_EDGE_TOP, true);
        gtk_layer_set_margin(GTK_WINDOW(gobj()), GTK_LAYER_SHELL_EDGE_TOP, 0);
        
        main_vbox.set_orientation(Gtk::ORIENTATION_VERTICAL);
        
        // Collapsed view
        collapsed_box.set_spacing(10); collapsed_box.set_margin_top(10); collapsed_box.set_margin_bottom(10);
        collapsed_box.set_margin_start(20); collapsed_box.set_margin_end(20);
        icon.set_pixel_size(32); collapsed_box.pack_start(icon, false, false, 0);
        label.set_text("🚀 Novic"); collapsed_box.pack_start(label, false, false, 0);
        visualizer_area.set_size_request(120, 36);
        visualizer_area.signal_draw().connect(sigc::mem_fun(*this, &NovicWindow::on_viz_draw));
        collapsed_box.pack_end(visualizer_area, false, false, 0);
        main_vbox.pack_start(collapsed_box, false, false, 0);
        
        // Expanded view
        expanded_box.set_orientation(Gtk::ORIENTATION_VERTICAL); expanded_box.set_spacing(15);
        expanded_box.set_margin_start(25); expanded_box.set_margin_end(25); 
        expanded_box.set_margin_top(20); expanded_box.set_margin_bottom(20);
        
        // Top row: Album art + Info + Visualizer
        top_row.set_spacing(20);
        album_art_area.set_size_request(90, 90);
        album_art_area.signal_draw().connect(sigc::mem_fun(*this, &NovicWindow::on_album_draw));
        top_row.pack_start(album_art_area, false, false, 0);
        
        // Middle section: text (expands)
        text_box.set_orientation(Gtk::ORIENTATION_VERTICAL); 
        text_box.set_valign(Gtk::ALIGN_CENTER); 
        text_box.set_spacing(6);
        text_box.set_hexpand(true);
        
        title_label.set_halign(Gtk::ALIGN_START); 
        title_label.set_ellipsize(Pango::ELLIPSIZE_END);
        title_label.set_hexpand(true);
        artist_label.set_halign(Gtk::ALIGN_START); 
        artist_label.set_ellipsize(Pango::ELLIPSIZE_END);
        artist_label.set_hexpand(true);
        
        text_box.pack_start(title_label, false, false, 0);
        text_box.pack_start(artist_label, false, false, 0);
        top_row.pack_start(text_box, true, true, 0);
        
        // Visualizer on right side of expanded view
        expanded_viz_area.set_size_request(80, 80);
        expanded_viz_area.set_valign(Gtk::ALIGN_CENTER);
        expanded_viz_area.signal_draw().connect(sigc::mem_fun(*this, &NovicWindow::on_expanded_viz_draw));
        top_row.pack_end(expanded_viz_area, false, false, 0);
        
        expanded_box.pack_start(top_row, false, false, 0);
        
        // Progress bar row
        progress_box.set_spacing(10);
        time_current.set_text("0:00"); time_current.set_halign(Gtk::ALIGN_START);
        time_remaining.set_text("-0:00"); time_remaining.set_halign(Gtk::ALIGN_END);
        progress_area.set_size_request(-1, 6); progress_area.set_hexpand(true);
        progress_area.signal_draw().connect(sigc::mem_fun(*this, &NovicWindow::on_progress_draw));
        progress_box.pack_start(time_current, false, false, 0);
        progress_box.pack_start(progress_area, true, true, 0);
        progress_box.pack_start(time_remaining, false, false, 0);
        expanded_box.pack_start(progress_box, false, false, 0);
        
        // Controls row
        controls_box.set_halign(Gtk::ALIGN_CENTER); controls_box.set_spacing(40);
        prev_btn.set_label("⏪"); play_btn.set_label("⏸"); next_btn.set_label("⏩");
        for (auto* b : {&prev_btn, &play_btn, &next_btn}) { b->set_relief(Gtk::RELIEF_NONE); b->set_size_request(60, 50); }
        prev_btn.signal_clicked().connect([this]() { media_monitor->previous(); });
        play_btn.signal_clicked().connect([this]() { media_monitor->play_pause(); });
        next_btn.signal_clicked().connect([this]() { media_monitor->next(); });
        controls_box.pack_start(prev_btn, false, false, 0);
        controls_box.pack_start(play_btn, false, false, 0);
        controls_box.pack_start(next_btn, false, false, 0);
        expanded_box.pack_start(controls_box, false, false, 0);
        
        main_vbox.pack_start(expanded_box, false, false, 0);
        add(main_vbox); show_all_children();
        icon.hide(); visualizer_area.hide(); expanded_box.hide();
        
        media_monitor = std::make_unique<MediaMonitor>();
        media_monitor->signal_media_changed().connect(sigc::mem_fun(*this, &NovicWindow::on_media_changed));
        audio_visualizer = std::make_unique<AudioVisualizer>();
        Glib::signal_timeout().connect([this]() { 
            audio_visualizer->update(); 
            if (is_playing) { 
                visualizer_area.queue_draw(); 
                expanded_viz_area.queue_draw();
                progress_area.queue_draw(); 
            } 
            return true; 
        }, 16);
        
        auto css = Gtk::CssProvider::create();
        css->load_from_data(
            "button { background: transparent; color: #666666; font-size: 28px; border: none; padding: 8px 16px; }"
            "button:hover { color: white; }"
            "label { color: white; }"
            ".time-label { color: #666666; font-size: 12px; }"
        );
        get_style_context()->add_provider_for_screen(Gdk::Screen::get_default(), css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        time_current.get_style_context()->add_class("time-label");
        time_remaining.get_style_context()->add_class("time-label");
    }

protected:
    bool on_enter_notify_event(GdkEventCrossing* e) override {
        if (e->detail != GDK_NOTIFY_INFERIOR && has_media) { 
            is_expanded = true; 
            collapsed_box.hide();  // Hide collapsed view
            expanded_box.show(); 
            resize(EXPANDED_WIDTH, EXPANDED_HEIGHT); 
        }
        return false;
    }
    bool on_leave_notify_event(GdkEventCrossing* e) override {
        if (e->detail != GDK_NOTIFY_INFERIOR) { 
            is_expanded = false; 
            expanded_box.hide(); 
            collapsed_box.show();  // Show collapsed view
            resize(COLLAPSED_WIDTH, COLLAPSED_HEIGHT); 
        }
        return false;
    }
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override {
        auto a = get_allocation(); double w = a.get_width(), h = a.get_height(), r = 25.0;
        cr->move_to(0, 0); cr->line_to(w, 0); cr->line_to(w, h - r);
        cr->arc(w - r, h - r, r, 0, M_PI / 2); cr->arc(r, h - r, r, M_PI / 2, M_PI); cr->close_path();
        cr->set_source_rgba(0.043, 0.047, 0.047, 0.98); cr->fill();
        return Gtk::Window::on_draw(cr);
    }
    bool on_viz_draw(const Cairo::RefPtr<Cairo::Context>& cr) {
        auto a = visualizer_area.get_allocation(); double w = a.get_width(), h = a.get_height();
        auto levels = audio_visualizer->get_levels();
        double bw = 3, bs = 3, tw = NUM_BARS * bw + (NUM_BARS - 1) * bs, sx = w - tw - 4, mh = h * 0.9, cy = h / 2;
        for (int i = 0; i < NUM_BARS; i++) {
            double bh = 3 + (mh - 3) * levels[i], x = sx + i * (bw + bs), y = cy - bh / 2, rad = bw / 2;
            cr->arc(x + rad, y + rad, rad, M_PI, 3 * M_PI / 2); cr->arc(x + bw - rad, y + rad, rad, 3 * M_PI / 2, 0);
            cr->arc(x + bw - rad, y + bh - rad, rad, 0, M_PI / 2); cr->arc(x + rad, y + bh - rad, rad, M_PI / 2, M_PI);
            cr->close_path(); cr->set_source_rgb(1, 1, 1); cr->fill();
        }
        return true;
    }
    bool on_expanded_viz_draw(const Cairo::RefPtr<Cairo::Context>& cr) {
        auto a = expanded_viz_area.get_allocation(); double w = a.get_width(), h = a.get_height();
        auto levels = audio_visualizer->get_levels();
        double bw = 4, bs = 4, tw = NUM_BARS * bw + (NUM_BARS - 1) * bs;
        double sx = (w - tw) / 2, mh = h * 0.85, cy = h / 2;
        for (int i = 0; i < NUM_BARS; i++) {
            double bh = 4 + (mh - 4) * levels[i], x = sx + i * (bw + bs), y = cy - bh / 2, rad = bw / 2;
            cr->arc(x + rad, y + rad, rad, M_PI, 3 * M_PI / 2); cr->arc(x + bw - rad, y + rad, rad, 3 * M_PI / 2, 0);
            cr->arc(x + bw - rad, y + bh - rad, rad, 0, M_PI / 2); cr->arc(x + rad, y + bh - rad, rad, M_PI / 2, M_PI);
            cr->close_path(); cr->set_source_rgb(1, 1, 1); cr->fill();
        }
        return true;
    }
    bool on_album_draw(const Cairo::RefPtr<Cairo::Context>& cr) {
        if (!album_pixbuf) return true;
        double s = 90, r = 10;
        cr->arc(r, r, r, M_PI, 3*M_PI/2); cr->arc(s-r, r, r, 3*M_PI/2, 0);
        cr->arc(s-r, s-r, r, 0, M_PI/2); cr->arc(r, s-r, r, M_PI/2, M_PI); cr->close_path(); cr->clip();
        Gdk::Cairo::set_source_pixbuf(cr, album_pixbuf, 0, 0); cr->paint();
        return true;
    }
    bool on_progress_draw(const Cairo::RefPtr<Cairo::Context>& cr) {
        auto a = progress_area.get_allocation(); double w = a.get_width(), h = a.get_height();
        double progress = (current_info.length > 0) ? (double)current_info.position / current_info.length : 0;
        progress = std::clamp(progress, 0.0, 1.0);
        cr->arc(h/2, h/2, h/2, M_PI/2, 3*M_PI/2); cr->arc(w-h/2, h/2, h/2, 3*M_PI/2, M_PI/2); cr->close_path();
        cr->set_source_rgb(0.2, 0.2, 0.2); cr->fill();
        double pw = std::max(h, w * progress);
        cr->arc(h/2, h/2, h/2, M_PI/2, 3*M_PI/2); cr->arc(pw-h/2, h/2, h/2, 3*M_PI/2, M_PI/2); cr->close_path();
        cr->set_source_rgb(0.5, 0.5, 0.5); cr->fill();
        return true;
    }
    void on_media_changed(MediaMonitor::MediaInfo info) {
        current_info = info; is_playing = info.is_playing;
        has_media = !info.title.empty() && info.title != "Unknown";
        audio_visualizer->set_playing(info.is_playing);
        if (has_media) {
            title_label.set_markup("<span font_weight='bold' font_size='x-large' foreground='white'>" + Glib::Markup::escape_text(info.title) + "</span>");
            std::string sub = info.artist;
            if (!info.album.empty() && info.album != "Unknown") sub += " • " + info.album;
            artist_label.set_markup("<span foreground='#888888'>" + Glib::Markup::escape_text(sub) + "</span>");
            play_btn.set_label(info.is_playing ? "⏸" : "▶");
            auto fmt = [](int64_t us) { int s = us / 1000000; int m = s / 60; s %= 60; char b[16]; snprintf(b, 16, "%d:%02d", m, s); return std::string(b); };
            time_current.set_text(fmt(info.position));
            if (info.length > 0) time_remaining.set_text("-" + fmt(info.length - info.position));
            load_album_art(info.art_url);
            if (!info.art_url.empty()) load_icon(info.art_url, 32);
            else load_app_icon(info.icon_name, info.player_name);
            label.hide(); visualizer_area.show();
        } else {
            icon.hide(); visualizer_area.hide(); label.set_text("🚀 Novic"); label.show();
            if (is_expanded) { is_expanded = false; expanded_box.hide(); collapsed_box.show(); resize(COLLAPSED_WIDTH, COLLAPSED_HEIGHT); }
        }
    }
private:
    Gtk::Box main_vbox{Gtk::ORIENTATION_VERTICAL}, expanded_box, text_box;
    Gtk::HBox collapsed_box, top_row, controls_box, progress_box;
    Gtk::Image icon; Gtk::Label label, title_label, artist_label, time_current, time_remaining;
    Gtk::Button prev_btn, play_btn, next_btn;
    Gtk::DrawingArea visualizer_area, album_art_area, progress_area, expanded_viz_area;
    Glib::RefPtr<Gdk::Pixbuf> album_pixbuf;
    std::unique_ptr<MediaMonitor> media_monitor; std::unique_ptr<AudioVisualizer> audio_visualizer;
    MediaMonitor::MediaInfo current_info; bool is_playing = false, is_expanded = false, has_media = false;
    
    void load_album_art(const std::string& url) {
        try {
            Glib::RefPtr<Gdk::Pixbuf> pb;
            if (url.find("file://") == 0) {
                std::string p = url.substr(7), d;
                for (size_t i = 0; i < p.length(); i++) { if (p[i] == '%' && i+2 < p.length()) { int v; std::stringstream ss; ss << std::hex << p.substr(i+1,2); ss >> v; d += (char)v; i += 2; } else d += p[i]; }
                pb = Gdk::Pixbuf::create_from_file(d, 90, 90, true);
            } else if (url.find("http") == 0) {
                pb = Gdk::Pixbuf::create_from_stream(Gio::File::create_for_uri(url)->read());
                if (pb) pb = pb->scale_simple(90, 90, Gdk::INTERP_BILINEAR);
            }
            if (pb) { album_pixbuf = pb; album_art_area.queue_draw(); }
        } catch (...) { album_pixbuf.reset(); }
    }
    void load_icon(const std::string& url, int size) {
        try {
            Glib::RefPtr<Gdk::Pixbuf> pb;
            if (url.find("file://") == 0) {
                std::string p = url.substr(7), d;
                for (size_t i = 0; i < p.length(); i++) { if (p[i] == '%' && i+2 < p.length()) { int v; std::stringstream ss; ss << std::hex << p.substr(i+1,2); ss >> v; d += (char)v; i += 2; } else d += p[i]; }
                pb = Gdk::Pixbuf::create_from_file(d, size, size, true);
            } else if (url.find("http") == 0) {
                pb = Gdk::Pixbuf::create_from_stream(Gio::File::create_for_uri(url)->read());
                if (pb) pb = pb->scale_simple(size, size, Gdk::INTERP_BILINEAR);
            }
            if (pb) { icon.set(pb); icon.show(); }
        } catch (...) {}
    }
    void load_app_icon(const std::string& name, const std::string& player) {
        auto t = Gtk::IconTheme::get_default();
        std::vector<std::string> icons = {name}; if (player.find("spotify")!=std::string::npos) icons.push_back("spotify"); icons.push_back("multimedia-player");
        for (auto& i : icons) { try { if (t->has_icon(i)) { auto pb = t->load_icon(i, 24, Gtk::ICON_LOOKUP_FORCE_SIZE); if (pb) { icon.set(pb); icon.show(); return; } } } catch(...){} }
    }
};

int main(int argc, char* argv[]) {
    auto app = Gtk::Application::create(argc, argv, "com.novic.app");
    NovicWindow w; return app->run(w);
}
