cars

baseline = ev26a;

flltd = 0.35:0.05:0.65;
flttd_comps = table();
for i = 1:length(flltd)
    m = flltd(i);
    car = baseline;
    car.flltd = m;
    comp = compete(car);
    baseline.base_power_limit = comp.car.base_power_limit;
    baseline.endur_power_limit = comp.car.endur_power_limit;
    flttd_comps = [flttd_comps; struct2table(comp)];
end

figure(Name="Overall vs LLTD")
plot(flltd, flttd_comps.score)
hold on
plot(flltd, flttd_comps.accel_score)
plot(flltd, flttd_comps.skid_score)
plot(flltd, flttd_comps.autox_score)
plot(flltd, flttd_comps.endur_score)
legend(["Total", "Accel", "Skid", "Autox", "Endur"])
xlabel("FLLTD")
ylabel("Competition points")

figure(Name="Events vs LLTD");
subplot(2,2,1);
plot(flltd, flttd_comps.accel_time);
title("Accel");
subplot(2,2,2);
plot(flltd, flttd_comps.skid_time);
title("Skid");
subplot(2,2,3);
plot(flltd, flttd_comps.autox_time);
title("Autox");
subplot(2,2,4);
plot(flltd, flttd_comps.endur_time);
title("Endur");