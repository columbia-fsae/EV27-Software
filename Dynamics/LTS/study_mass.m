cars

baseline = ev26a;

mass = baseline.mass + (-40:10:40);
mass_comps = table();
for i = 1:length(mass)
    m = mass(i);
    car = baseline;
    car.mass = m;
    comp = compete(car);
    baseline.base_power_limit = comp.car.base_power_limit;
    baseline.endur_power_limit = comp.car.endur_power_limit;
    mass_comps = [mass_comps; struct2table(comp)];
end

figure(Name="Overall vs Mass")
plot(mass, mass_comps.score)
hold on
plot(mass, mass_comps.accel_score)
plot(mass, mass_comps.skid_score)
plot(mass, mass_comps.autox_score)
plot(mass, mass_comps.endur_score)
legend(["Total", "Accel", "Skid", "Autox", "Endur"])
xlabel("Mass with driver (kg)")
ylabel("Competition points")

figure(Name="Events vs Mass");
subplot(2,2,1);
plot(mass, mass_comps.accel_time);
title("Accel");
subplot(2,2,2);
plot(mass, mass_comps.skid_time);
title("Skid");
subplot(2,2,3);
plot(mass, mass_comps.autox_time);
title("Autox");
subplot(2,2,4);
plot(mass, mass_comps.endur_time);
title("Endur");