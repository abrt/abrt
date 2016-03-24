module Abrt =
    autoload xfm

    let lns = Libreport.lns

    let filter = (incl "/etc/abrt/*" )
               . (excl "/etc/abrt/plugins")
               . (incl "/etc/abrt/plugins/*")
               . (incl "/usr/share/abrt/conf.d/*")
               . (excl "/usr/share/abrt/conf.d/plugins")
               . (incl "/usr/share/abrt/conf.d/plugins/*")
               . Util.stdexcl

   let xfm = transform lns filter
