#define SDL_MAIN_HANDLER
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <thread>
#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <conio.h>
  typedef SOCKET sock_t;
#else
  #include <arpa/inet.h>
  #include <sys/socket.h>
  #include <termios.h>
  #include <unistd.h>
  typedef int sock_t;
#endif
#include <SDL2/SDL.h>
#include "net_fdm.hpp"

// FlightGear FDM Packet CRC Check
static_assert(sizeof(FGNetFDM) == 408, "FGNetFDM Size Mismatch");

static void swapN(unsigned char *p, int n) {
  for (int i = 0; i < n/2; i++){
    std::swap(p[i], p[n - 1 - i]);
  }
}

static void to_network(FGNetFDM& f){
  unsigned char* p = (unsigned char*)&f;
  swapN(p, 4);
  swapN(p + 4, 4);
  for (int i = 0; i < 3; i++){
    swapN(p + 8 + 8*i, 8);
  }
  for (size_t o = 32; o < sizeof(f); o += 4){
    swapN(p + o, 4);
  }
}

// Aircraft Model
constexpr double PI = 3.14159265358979, G = 9.80665, RE = 6378137.0, FT = 3.280839895;
enum { U, V, W, P, Q, R, PHI, THE, PSI, PN, PE, PD };
using State = std::array<double, 12>;
struct Controls { double de, da, dr, thr; }; // rad, rad, rad, 0..1

struct Aircraft {
  double m = 1043, Ixx = 1285, Iyy = 1825, Izz = 2667;
  double S = 16.2, b = 11.0, chord = 1.49, Pmax = 96e3;
  double alt0 = 1000;     //Initial Altitude [m]

// Aerodynamic Coefficients for Cessna 172
  double CL0 = 0.31, CLa = 4.6, CLq = 3.9, CLde = 0.43;
  double CD0 = 0.031, K = 0.054;
  double Cm0 = -0.015, Cma = -0.89, Cmq = -12.4, Cmde = -1.28;
  double CYb = -0.31, CYdr = 0.187;
  double Crb = -0.089, Crp = -0.47, Crr = 0.096, Crda = 0.178, Crdr = 0.0147;
  double Cnb = 0.065, Cnp = -0.03, Cnr = -.099, Cnda = -0.053, Cndr = 0.0657;

  double rho(double h) const { return 1.225 * std::pow(1 - 2.25577e-5*h, 4.2559); };

  State deriv(const State& x, const Controls& c) const {
    double V2 = x[U]*x[U] + x[V]*x[V] + x[W]*x[W];
    double Va = std::max(std::sqrt(V2), 1.0);
    double al = std::atan2(x[W], x[U]), be = std::asin(x[V] / Va);
    double qb = 0.5 * rho(alt0 - x[PD]) * V2;
    double pb = x[P]*b / (2*Va), qc = x[Q]*chord / (2*Va), rb = x[R]*b / (2*Va);

    double CL = CL0 + CLa*al + CLq*qc + CLde*c.de;
    double CD = CD0 + K*CL*CL;
    double CY = CYb*be + CYdr*c.dr;
    double Cr = Crb*be + Crp*pb + Crr*rb + Crda*c.da + Crdr*c.dr;
    double Cm = Cm0+ Cma*al + Cmq*qc + Cmde*c.de;
    double Cn = Cnb*be + Cnp*pb + Cnr*rb + Cnda*c.da + Cndr*c.dr;

    double L = qb*S*CL, D = qb*S*CD, Y = qb*S*CY;
    double T = c.thr * Pmax / std::max(Va, 20.0);
    double Fx = L*std::sin(al) - D*std::cos(al) + T;
    double Fy = Y;
    double Fz = -L*std::cos(al) - D*std::sin(al);

    double sp = std::sin(x[PHI]), cp = std::cos(x[PHI]);
    double st = std::sin(x[THE]), ct = std::cos(x[THE]);
    double ss = std::sin(x[PSI]), cs = std::cos(x[PSI]);

    State d{};
    d[U] = x[R]*x[V] - x[Q]*x[W] + Fx/m - G*st;
    d[V] = x[P]*x[W] - x[R]*x[U] + Fy/m + G*sp*ct;
    d[W] = x[Q]*x[U] - x[P]*x[V] + Fz/m + G*cp*ct;
    d[P] = (qb*S*b*Cr - (Izz - Iyy)*x[Q]*x[R]) / Ixx;
    d[Q] = (qb*S*chord*Cm - (Ixx - Izz)*x[P]*x[R]) /Iyy;
    d[R] = (qb*S*b*Cn - (Iyy - Ixx)*x[P]*x[Q]) /Izz;
    d[PHI] = x[P] + (x[Q]*sp + x[R]*cp) * st/ct;
    d[THE] = x[Q]*cp - x[R]*sp;
    d[PSI] = (x[Q]*sp + x[R]*cp) / ct;
    d[PN] = x[U]*ct*cs + x[V]*(sp*st*cs - cp*ss) + x[W]*(cp*st*cs + sp*ss);
    d[PE] = x[U]*ct*ss + x[V]*(sp*st*ss + cp*cs) + x[W]*(cp*st*ss - sp*cs);
    d[PD] = -x[U]*st + x[V]*sp*ct + x[W]*cp*ct;
    return d;
  }
    void step(State& x, const Controls& c, double dt) const
    { //RK4
      auto add = [](const State& a, const State& k, double h) {
        State r;
        for (int i = 0; i < 12; i++) r[i] = a[i] + h*k[i];
        return r;
      };
      
      State k1 = deriv(x, c), k2 = deriv(add(x, k1, dt/2), c);
      State k3 = deriv(add(x, k2, dt/2), c), k4 = deriv(add(x, k3, dt), c);
      
      for (int i = 0; i < 12; i++) x[i] += dt/6 * (k1[i] + 2*k2[i] + 2*k3[i] + k4[i]);
    }

    // level-flight trim at airspeed Va [m/s]
    void trim(double Va, State& x, Controls& c) const {
      double qb = 0.5 * rho(alt0) * Va*Va;
      double CLreq = m*G /(qb*S);
      double det = CLa*Cmde - CLde*Cma;
      double al = ((CLreq -CL0)*Cmde + CLde*Cm0) / det;
      double de = (-CLa*Cm0 - Cma*(CLreq - CL0)) /det;
      double CD = CD0 + K*CLreq*CLreq;
      x = {};
      x[U] = Va*std::cos(al); x[W] = Va*std::sin(al);
      x[THE] = al; x[PSI] = PI/2; // heading east
      c = {de, 0, 0, std::min(1.0, qb*S*CD*Va / Pmax)};
    }
};

// ---- non-blocking keyboard ----
static volatile std::sig_atomic_t g_quit = 0;
#ifdef _WIN32
static void kb_init() {}
static void kb_close() {}
static int kb_get() { return _kbhit() ? _getch() : -1; }
#else
static termios old_tio;
static void kb_init() {
  tcgetattr(0, &old_tio);
  termios t = old_tio;
  t.c_lflag &= ~(ICANON | ECHO);
  t.c_cc[VMIN] = 0; t.c_cc[VTIME] = 0;      // read() returns immediately
  tcsetattr(0, TCSANOW, &t);
}
static void kb_close() { tcsetattr(0, TCSANOW, &old_tio); }
static int kb_get() { unsigned char c; return read(0, &c, 1) == 1 ? c : -1; }
#endif

// ---- gamepad helper: normalize + deadzone ----
static double dz(Sint16 v) {
  double x = v / 32767.0;
  if (std::fabs(x) < 0.08) return 0;
  return std::clamp(x, -1.0, 1.0);
}

int main() {
#ifdef _WIN32
  WSADATA w; WSAStartup(MAKEWORD(2, 2), &w);
#endif
  sock_t s = socket(AF_INET, SOCK_DGRAM, 0);
  sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(5500);
  inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

  Aircraft ac; State x; Controls ctl;
  ac.trim(55.0, x, ctl);
  const Controls trimmed = ctl;
  const double lat0 = 41.0 * PI/180, lon0 = 28.8 * PI/180;
  const double dt = 1.0 / 60;
  auto next = std::chrono::steady_clock::now();

  kb_init();
  std::signal(SIGINT, [](int) { g_quit = 1; });

  SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
  SDL_Init(SDL_INIT_GAMECONTROLLER);
  SDL_GameController* pad = nullptr;
  for (int i = 0; i < SDL_NumJoysticks(); i++)
    if (SDL_IsGameController(i)) { pad = SDL_GameControllerOpen(i); break; }
  if (pad) std::printf("Controller: %s\n", SDL_GameControllerName(pad));
  else     std::printf("No controller found, keyboard only\n");
  std::printf("Keys: W/S pitch  A/D roll  Q/E rudder  R/F throttle  Space=center  Esc=quit\n");
  std::printf("Pad: left stick pitch/roll, right stick X rudder, RT/LT throttle, Start=quit\n");

  double de_in = 0, da_in = 0, dr_in = 0, thr = trimmed.thr;

  for (double t = 0; !g_quit; t += dt) {
    // ---- keyboard ----
    int k;
    while ((k = kb_get()) != -1) {
      switch (std::tolower(k)) {
        case 'w': de_in += 0.01; break;     // stick forward -> nose down
        case 's': de_in -= 0.01; break;     // stick back -> nose up
        case 'a': da_in -= 0.01; break;
        case 'd': da_in += 0.01; break;
        case 'q': dr_in -= 0.02; break;
        case 'e': dr_in += 0.02; break;
        case 'r': thr += 0.05; break;
        case 'f': thr -= 0.05; break;
        case ' ': de_in = da_in = dr_in = 0; break;
        case 27:  g_quit = 1; break;        // Esc
      }
    }
    de_in = std::clamp(de_in, -0.30, 0.30);
    da_in = std::clamp(da_in, -0.30, 0.30);
    dr_in = std::clamp(dr_in, -0.40, 0.40);
    thr   = std::clamp(thr, 0.0, 1.0);

    // ---- gamepad ----
    double p_de = 0, p_da = 0, p_dr = 0;
    if (pad) {
      SDL_GameControllerUpdate();
      p_de = -dz(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY)) * 0.30;
      p_da =  dz(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX)) * 0.30;
      p_dr =  dz(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX)) * 0.40;
      double rt = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) / 32767.0;
      double lt = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) / 32767.0;
      thr = std::clamp(thr + (rt - lt) * 0.5 * dt, 0.0, 1.0);
      if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START)) g_quit = 1;
    }
    ctl = { trimmed.de + std::clamp(de_in + p_de, -0.30, 0.30),
            std::clamp(da_in + p_da, -0.30, 0.30),
            std::clamp(dr_in + p_dr, -0.40, 0.40), thr };

    // ---- physics ----
    ac.step(x, ctl, dt);
    State d = ac.deriv(x, ctl);
    double Va = std::sqrt(x[U]*x[U] + x[V]*x[V] + x[W]*x[W]);

    // ---- build + send FlightGear packet ----
    FGNetFDM f; std::memset(&f, 0, sizeof f);
    f.version = 24;
    f.latitude  = lat0 + x[PN] / RE;
    f.longitude = lon0 + x[PE] / (RE * std::cos(lat0));
    f.altitude  = ac.alt0 - x[PD];
    f.agl = (float)f.altitude;
    f.phi = x[PHI]; f.theta = x[THE]; f.psi = x[PSI];
    f.alpha = std::atan2(x[W], x[U]); f.beta = std::asin(x[V] / Va);
    f.phidot = d[PHI]; f.thetadot = d[THE]; f.psidot = d[PSI];
    f.vcas = Va * 1.94384;
    f.climb_rate = -d[PD] * FT;
    f.v_north = d[PN]*FT; f.v_east = d[PE]*FT; f.v_down = d[PD]*FT;
    f.v_body_u = x[U]*FT; f.v_body_v = x[V]*FT; f.v_body_w = x[W]*FT;
    f.num_engines = 1; f.rpm[0] = 700 + 1900 * ctl.thr;
    f.num_wheels = 3; for (int i = 0; i < 3; i++) f.gear_pos[i] = 1;
    f.cur_time = (uint32_t)std::time(nullptr); f.visibility = 20000;
    f.elevator = ctl.de / 0.44; f.rudder = ctl.dr / 0.44;
    f.left_aileron = -ctl.da / 0.35; f.right_aileron = ctl.da / 0.35;

    to_network(f);
    sendto(s, (const char*)&f, sizeof f, 0, (sockaddr*)&addr, sizeof addr);

    if (std::fmod(t, 1.0) < dt)
      std::printf("t=%5.1f  V=%5.1f m/s  alt=%6.1f m  roll=%5.1f  pitch=%4.1f  hdg=%5.1f  thr=%3.0f%%\n",
        t, Va, ac.alt0 - x[PD], x[PHI]*180/PI, x[THE]*180/PI,
        std::fmod(x[PSI]*180/PI + 360, 360), ctl.thr * 100);

    next += std::chrono::microseconds((int)(dt * 1e6));
    std::this_thread::sleep_until(next);
  }

  if (pad) SDL_GameControllerClose(pad);
  SDL_Quit();
  kb_close();
  return 0;
}


