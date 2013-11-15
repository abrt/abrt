module Abrt =
    autoload xfm

    let lns = Libreport.lns

    let filter = (incl "/etc/abrt/*" )
               . (incl "/etc/abrt/plugins/*")
               . (incl "/usr/share/abrt/conf.d/*")
               . (incl "/usr/share/abrt/conf.d/plugins/*")
               . Util.stdexcl

   let xfm = transform lns filter
