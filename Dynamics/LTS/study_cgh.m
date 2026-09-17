cars

baseline = ev26a;

cgh = baseline.cg(3) * 1000 + flip(-40:10:40);
cgh_comps = table();
for i = 1:length(cgh)
    m = cgh(i);
    car = baseline;
    car.cg(3) = m / 1000;
    comp = compete(car);
    baseline.base_power_limit = comp.car.base_power_limit;
    baseline.endur_power_limit = comp.car.endur_power_limit;
    cgh_comps = [cgh_comps; struct2table(comp)];
end

figure(Name="Overall vs CGH")
plot(cgh, cgh_comps.score)
hold on
plot(cgh, cgh_comps.accel_score)
plot(cgh, cgh_comps.skid_score)
plot(cgh, cgh_comps.autox_score)
plot(cgh, cgh_comps.endur_score)
legend(["Total", "Accel", "Skid", "Autox", "Endur"])
xlabel("CGH (mm)")
ylabel("Competition points")

figure(Name="Events vs CGH");
subplot(2,2,1);
plot(cgh, cgh_comps.accel_time);
title("Accel");
subplot(2,2,2);
plot(cgh, cgh_comps.skid_time);
title("Skid");
subplot(2,2,3);
plot(cgh, cgh_comps.autox_time);
title("Autox");
subplot(2,2,4);
plot(cgh, cgh_comps.endur_time);
title("Endur");