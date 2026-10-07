cars
tracks

cara = ev26a;
carb = ev26b;

% %% Accel Experiment
% t = accel_event;
% sima = lapsim(t, cara);
% simb = lapsim(t, carb);
% accel = [sima simb];
% figure(Name="Acceleration");
% plot_lap(sima, simb);
% accel_times = arrayfun(@(s) s.time, accel)
%
% %% Skidpad Experiment
% t = skid_event;
% sima = lapsim(t, cara);
% simb = lapsim(t, carb);
% skid = [sima simb];
% figure(Name="Skidpad");
% plot_lap(sima, simb);
% skid_times = arrayfun(@(s) s.time, skid)

%% Autocross Experiment
t = autox_event;
sima = lapsim(t, cara);
simb = lapsim(t, carb);
autox = [sima simb];
figure(Name="Autocross");
plot_lap(sima, simb);
autox_times = arrayfun(@(s) s.time, autox)

% %% Endurance Experiment
% t = endur_event;
% sima = lapsim(t, cara);
% simb = lapsim(t, carb);
% endur = [sima simb];
% figure(Name="Endurance");
% plot_lap(sima, simb);
% endur_times = arrayfun(@(s) s.time * 22, endur)

%% Competition Points
score26a = lap2score(accel_times(1), skid_times(1), endur_times(1) * 0.8/22, endur_times(1));
score26b = lap2score(accel_times(2), skid_times(2), endur_times(2) * 0.8/22, endur_times(2));

%% Plotting
function plot_lap(sima, simb)
    M = 5;
    N = 2;
    P = 1;
    subplot(M,N,P); P = P + 1;
    plot(sima.xx(sima.ii), sima.vv(sima.ii) * 3.6);
    xlabel("Distance (m)")
    ylabel("Velocity (km/h)")
    hold on
    plot(simb.xx(simb.ii), simb.vv(simb.ii) * 3.6);
    hold off

    subplot(M,N,P); P = P + 1;
    plot(sima.xx(sima.ii), sima.stats.pbrakes(sima.ii) * 1e-3);
    xlabel("Distance (m)")
    ylabel("Brakes (kW)")
    hold on
    plot(simb.xx(simb.ii), simb.stats.pbrakes(simb.ii) * 1e-3);
    legend(["A" "B"])
    hold off

    subplot(M,N,P); P = P + 1;
    plot(sima.xx(sima.ii), sima.stats.ptraction(sima.ii) * 1e-3);
    xlabel("Distance (m)")
    ylabel("Tractive Power (kW)")
    hold on
    plot(simb.xx(simb.ii), simb.stats.ptraction(simb.ii) * 1e-3);
    hold off

    subplot(M,N,P); P = P + 1;
    plot(sima.xx(sima.ii), sima.stats.pelectric(sima.ii) * 1e-3);
    xlabel("Distance (m)")
    ylabel("Electric Power (kW)")
    hold on
    plot(simb.xx(simb.ii), simb.stats.pelectric(simb.ii) * 1e-3);
    hold off

    subplot(M,N,P); P = P + 1;
    plot(sima.xx(sima.ii), sima.stats.ax(sima.ii) / 9.806);
    xlabel("Distance (m)")
    ylabel("Longitudinal G Force (G)")
    ylim([-2 2])
    hold on
    plot(simb.xx(simb.ii), simb.stats.ax(simb.ii) / 9.806);
    hold off

    subplot(M,N,P); P = P + 1;
    plot(sima.xx(sima.ii), sima.stats.ay(sima.ii) / 9.806);
    xlabel("Distance (m)")
    ylabel("Lateral G Force (G)")
    ylim([-2 2])
    hold on
    plot(simb.xx(simb.ii), simb.stats.ay(simb.ii) / 9.806);
    hold off

    subplot(M,N,P); P = P + 1;
    track_map(sima, sima.vv);
    title("A " + sima.time)

    subplot(M,N,P); P = P + 1;
    track_map(simb, simb.vv);
    title("B " + simb.time)

    subplot(M,N,P); P = P + 1;
    plot(sima.xx(sima.ii), sima.stats.ay(sima.ii) / 9.806);
    xlabel("Distance (m)")
    ylabel("Corner Loads (N)")
    ylim([-2 2])
    hold on
    %plot(simb.xx(simb.ii), simb.stats.ay(simb.ii) / 9.806);
    hold off
end
