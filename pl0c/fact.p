var p;

procedure factorial(n)
    begin
        p = 1;
        while n > 1 do begin
            p = p * n;
            n = n - 1
        end
    end;

begin
    factorial(10)
end.