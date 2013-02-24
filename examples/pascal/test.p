PROCEDURE Error
BEGIN  IF AnotherError THEN
        BEGIN  Fault:=12.1 - Errorcode
        END
END { of Error }
