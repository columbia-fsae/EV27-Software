clear variables
% ---- Given (your numbers) ----
P.tw      = 1.2;        % track width [m]
P.w_bias  = 0.5;        % front weight fraction (0.5 = 50/50)
P.cgh     = 0.247;      % CG height [m]


% ---- Mass / environment (SET THESE to your car) ----
P.m_total = 264;        % total mass incl. driver [kg]
P.g       = 9.81;       % gravity [m/s^2]
P.rho     = 1.20;       % air density [kg/m^3]

% ---- Suspension / tyre vertical (per corner) ----
P.k_t     = 110e3;      % tyre vertical stiffness [N/m] (~90-130 N/mm) <-- CONFIRM
                        % body-mode damping ratio (held constant in sweep)

% ---- Spring (wheel-rate) sweep -- the ONLY swept lever ----
P.kw_front_base = 3.7843e4; % baseline FRONT wheel rate [N/m]
P.kw_rear_base  = 3.7843e4; % baseline REAR  wheel rate [N/m]

% ---- Road spectrum (ISO 8608). Autocross asphalt ~ class A-B ----
P.Gd_n0 = 4e-6;         % roughness PSD at n0 [m^3]  (A~1e-6, B~4e-6, C~16e-6)
P.n0    = 0.1;          % reference spatial frequency [cyc/m]
P.w_iso = 2.0;          % PSD waviness exponent
P.n_lo  = 0.02;  P.n_hi = 12;  P.n_pts = 4000;   % spatial-freq integration grid


P.m_u = 5.7;         % unsprung mass per corner [kg]
P.kw_front_base = 3.7843e4;
P.m_s = P.m_total/4 - P.m_u;
P.zeta = 0.3;

P.V_op = 15;


%% CPL parameter sweep
sweep_arr = linspace(0.1, 1.2, 20);
% sweep_arr = linspace(0.1, 2, 20) .* 3.7843e4;
% sweep_arr = linspace(0.7, 1.5, 20) .* 5.7;
sig_arr = zeros(length(sweep_arr),1);
i = 1;
for x = sweep_arr
    P.zeta = x;
    % P.kw_front_base = x;

    % P.m_u = x;
    % P.m_s = P.m_total/4 - P.m_u;

    sig_arr(i) = sigma_Fz(P.kw_front_base, P.m_s, P);
    i = i + 1;
end

set(groot, 'defaultAxesFontSize', 16)

figure()
plot(sweep_arr, sig_arr, LineWidth=3)
xlabel("\zeta")
% xlabel("Front Spring Rate")
% xlabel("Unsprung Mass")
ylabel("\sigma_{F_z}[N]", Rotation=0)
title("Sensitivity of CPL Variation on Crit Damping %")

%% Effective mu parameter sweep
Fz_grid = linspace(100, 1600, 160);
k_sweep = linspace(0, 0.25, 60);        % slip-ratio sweep for longitudinal peak
a_sweep = deg2rad(linspace(0, 14, 60)); % slip-angle  sweep for lateral peak

FxMax = zeros(size(Fz_grid));           % peak |Fx| vs Fz [N]
FyMax = zeros(size(Fz_grid));           % peak |Fy| vs Fz [N]
for i = 1:numel(Fz_grid)
    fz = Fz_grid(i);
    fx = zeros(size(k_sweep));
    for j = 1:numel(k_sweep)
        fxj = brushTireForce(k_sweep(j), 0, -fz);   % pure longitudinal (a=0)
        fx(j) = abs(fxj);
    end
    FxMax(i) = max(fx);
    fy = zeros(size(a_sweep));
    for j = 1:numel(a_sweep)
        [~, fyj] = brushTireForce(0, a_sweep(j), -fz); % pure lateral (k=0)
        fy(j) = abs(fyj);
    end
    FyMax(i) = max(fy);
end

mu_sweep_arr = linspace(1e4, 6e4, 20);

idx = 1;
car_eff_mu_arr = zeros(length(mu_sweep_arr), 1);
for x=mu_sweep_arr
    P.kw_front_base = x;
    sigma_baseline = sigma_Fz(P.kw_front_base, P.m_s, P);
    car_eff_mu_arr(idx) = eff_mu(P.m_total * P.g/4, sigma_baseline, Fz_grid, FyMax);
    idx = idx + 1;
end

figure()
plot(mu_sweep_arr, car_eff_mu_arr)
xlabel("Ride Rate")
ylabel("Effective \mu")

%%

function sg = sigma_Fz(kw, m_s, P)
% RMS dynamic tyre-load variation from a quarter-car driven by an ISO-8608
% road PSD. kw = wheel (suspension) rate [N/m]; m_s = corner sprung mass [kg].
    k_t = P.k_t; m_u = P.m_u;
    k_ride = kw*k_t/(kw+k_t);
    c = 2*P.zeta*sqrt(k_ride*m_s);              % body-mode damping, constant zeta

    n  = linspace(P.n_lo, P.n_hi, P.n_pts);     % spatial frequency [cyc/m]
    Gd = P.Gd_n0*(n/P.n0).^(-P.w_iso);          % road displacement PSD [m^3]

    w  = 2*pi*P.V_op*n;                          % temporal angular freq [rad/s]
    s  = 1i*w;
    A  = kw + c*s;
    Den = (m_u*s.^2 + A + k_t).*(m_s*s.^2 + A) - A.^2;
    Zu_Zr = k_t.*(m_s*s.^2 + A)./Den;            % road->unsprung motion
    H_Fz  = k_t.*(Zu_Zr - 1);                    % road->dynamic tyre force [N/m]

    PSD_Fz = (abs(H_Fz).^2).*Gd;                 % output PSD over spatial freq
    var_Fz = trapz(n, PSD_Fz);                   % variance [N^2]
    sg = sqrt(max(var_Fz,0));
end

function mu_e = eff_mu(Fz_mean, sigma, Fz_grid, Fmax_grid)
% Effective peak grip = E[ Fmax(Fz) ] / Fz_mean, averaging the tyre's peak-force
% curve over a (truncated-normal) load distribution of mean Fz_mean, std sigma.
% Concavity of Fmax(Fz) (load sensitivity) makes variation reduce the average.
    if sigma < 1e-6
        mu_e = interp1(Fz_grid, Fmax_grid, Fz_mean,'linear','extrap')/Fz_mean;
        return;
    end
    lo = max(Fz_mean-5*sigma, 1);  hi = Fz_mean+5*sigma;
    z  = linspace(lo, hi, 401);
    pdf = exp(-0.5*((z-Fz_mean)/sigma).^2);
    pdf = pdf/trapz(z,pdf);                       % renormalise (truncation)
    Fm = interp1(Fz_grid, Fmax_grid, z,'linear','extrap');
    Fm = max(Fm,0);
    Fbar = trapz(z, Fm.*pdf);                      % expected peak force [N]
    mu_e = Fbar/Fz_mean;
end
