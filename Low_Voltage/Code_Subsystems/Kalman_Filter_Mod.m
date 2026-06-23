classdef Kalman_Filter_Mod < matlab.System
    % untitled3 Add summary here
    %
    % This template includes the minimum set of functions required
    % to define a System object.

    % Public, tunable properties
    properties
       qgain=1; 
    end

    % Pre-computed constants or internal states
    properties (Access = private)
        % State-space matrices
        
        a = [1 35.3*0.002;0 1]; 
        b = [0.002;0]; 
        c_v = [1 0];
        c_a = [0 1];
        k=1;
        % Error covariance matrices
        q_fixed;   % Process noise
        r_v;
        r_a =  0.00023 ; % Sensor noise
        
        % Initial conditions
        x
        p
        i
        n_a
        n_v
        predX
        predP
        updateX
        updateK
        updateP
        t
       
    end

    methods (Access = protected)
        function setupImpl(obj)
            % Perform one-time calculations, such as computing constants
            obj.x = [0;0]; % Initial state, v,a
            obj.p = [0 0;0 0]; % Initial error covariance
            obj.i=1;
            obj.updateK = @(p,c,r) (c*p*c' + r)\p*c';      % K[k]   = PC^T/(CPC^T + R)
            obj.updateX = @(x,k,z,c) x + k*(z-c*x);        % x[n|n] = x[n|n-1] + K[n](z[n] - Cx[n|n-1])
            obj.updateP = @(k,c,p) (eye(size(p)) - k*c)*p; % P[n|n] = (I-K[n]C)P[n|n-1]
            obj.r_v = @(v) min(((21*1.5)/(2*v*3.6*pi*0.203))^2, 1^2);
            % Time updates
            obj.predX = @(a,x,b,u) a*x + b*u;            % x[n+1|n] = Ax[n] + Bu[n]
            obj.predP = @(a,p,q) a*p*a' + q;        % P[n|n]   = AP[n|n]A^T + BQB^T
            obj.n_v = 0;
            obj.n_a = 0;
            obj.t = 0;
        end

        function [v,a,cov_v] = stepImpl(obj, a, v_f,n_a,n_v,t_new)
            % Implement algorithm. Calculate y as a function of input u and
            % internal states.
            obj.a = [1 35.3*(t_new-obj.t);0 1];
            obj.b = [(t_new-obj.t);0];
            obj.q_fixed = ([(35.3*(t_new-obj.t)*1.5/2)^2 0;0 (1.5/2)^2]); % set 2 sigma at max accel, 1.5G
            obj.t = t_new;
            q=obj.qgain*obj.q_fixed;
            obj.x = obj.predX(obj.a,obj.x,[0;0],0);
            obj.p = obj.predP(obj.a,obj.p,q);
            if obj.n_v ~= n_v
                z_v = v_f;
                obj.k = obj.updateK(obj.p, obj.c_v, obj.r_v(z_v));
                obj.x = obj.updateX(obj.x, obj.k, z_v, obj.c_v);
                obj.p = obj.updateP(obj.k, obj.c_v, obj.p);
                obj.n_v = n_v;
            end
        
            if obj.n_a ~= n_a
                obj.k = obj.updateK(obj.p, obj.c_a, obj.r_a);
                obj.x = obj.updateX(obj.x, obj.k, -a, obj.c_a);
                obj.p = obj.updateP(obj.k, obj.c_a, obj.p);
                obj.n_a = n_a;
            end

            if obj.c_v*obj.x < 0
                obj.x(1) = 0;
            end

            obj.i=obj.i+1;
            v= obj.c_v*obj.x;
            cov_v = sqrt(obj.c_v*obj.p*obj.c_v');
            a = obj.c_a*obj.x;
            
          
        end

        function resetImpl(~)
            % Initialize / reset internal properties
            %obj.x = [0;0]; % Initial state, v,a
            %obj.p = [0 0;0 0]; % Initial error covariance
            %obj.i=1;
        end
    end
end
