module Abrt =
    autoload xfm

    let lns = Libreport.lns

    let filter = (incl "/etc/abrt/*.conf" )
               . (incl "/etc/abrt/plugins/*")
               . Util.stdexcl

   let xfm = transform lns filter
