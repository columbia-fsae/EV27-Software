tw = 1.2;
w_bias = 0.5;
cgh = 0.247;
FLLTD = 0.5;
cla = 3.1;
cp = 0.4;
toe_f = 0;
toe_r = 0;
v = 10;
ax_target = 0;
b_bias = 0.5;

figure()
hold on
N = 10;
toe_f_list = linspace(-1.5, 1, N); % in degrees
toe_r_list = linspace(-1.5, 1, N);
b_bias_list = linspace(0.1, 0.9, N);
w_bias_list = linspace(0.3, 0.7, N);
M = 7;
ax_target_list = linspace(-1.1, 0.7, M);


N_control_list = zeros(N, 1);
N_stability_list = zeros(N,1);

for j = 1:M
    for i = 1:N
        ymd = YMDSolver(tw, w_bias_list(i), cgh, v, ax_target_list(j), FLLTD, cla, cp, toe_f, toe_r, 0.5);
        [N_stability_list(i), N_control_list(i)] = ymd.cs_derivatives(0, 0);
    end
    disp_string = sprintf("Ax = %.2f g", ax_target_list(j));
    plot(w_bias_list, N_control_list, LineWidth=2,DisplayName=disp_string)
end
%%


xlabel("Weight Bias % Front", FontSize=14)
ylabel('N_{\delta} (Nm/deg)', Rotation=0, FontSize=14)
title("Weight Bias Effects on Controls at Different Cornering Stages", FontSize=16)
legend()
set(gca, 'FontSize', 14)

grid on


%%
% tw = 1.2;
% w_bias = 0.5;
% cgh = 0.247;
% FLLTD = 0.5;
% cla = 3.1;
% cp = 0.4;
% toe_f = 0;
% toe_r = 0;
% v = 10;
% ax_target = 0;
% b_bias = 0.5;
% ymd_0 = YMDSolver(tw, 0.5, cgh, v, 0, FLLTD, cla, cp, toe_f, toe_r, b_bias);
% [ay_0, mz_0, fy_0, fz_0] = ymd_0.solve_combined_slip(0, 1);
%
% ymd_1 = YMDSolver(tw, 0.5, cgh, v, 0.26, FLLTD, cla, cp, toe_f, toe_r, b_bias);
% [ay_1, mz_1, fy_1, fz_1] = ymd_1.solve_combined_slip(0, 1);
%
% fprintf("Mz's: %d, %d \n", mz_0, mz_1)
% disp("Fzs:")
% disp(fz_0)
% disp(fz_1)
%
% disp("fys:")
% disp(fy_0)
% disp(fy_1)

% %% YMD plot
% tw = 1.2;
% w_bias = 0.5;
% cgh = 0.247;
% FLLTD = 0.5;
% cla = 3.1;
% cp = 0.4;
% toe_f = 0;
% toe_r = 0;
% v = 10;
% ax_target = 0;
% b_bias = 0.5;
%
% ymd = YMDSolver(tw, 0.5, cgh, v, 0, FLLTD, cla, cp, toe_f, toe_r, 0.5);
% ymd.ymd_plot('blue')
