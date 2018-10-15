Release\executor - файл-контейнер для java, в него в виде zip-архива добавляется java.exe с необходимыми jar-библиотеками. stdout от java сохраняется на диск в %temp%\infomaximum_stdout.log

Release\patcher добавляет нужные ресурсы в executor. patcher без аргументов - выводит список допустимых опций

options:
  --executor-path arg   path to executable file, utf-8
  --icon-path arg       [optional] path to icon, *.ico format
  --company-name arg    [optional] CompanyName, utf-8
  --description arg     [optional] FileDescription, utf-8
  --version arg         [optional] Version, v1[.v2[.v3[.v4]]]
  --copyright arg       [optional] Copyright, utf-8
  --product-name arg    [optional] ProductName, utf-8
  --run-as-admin arg    [optional] RunAsAdmin, true/false, default=false
  --string-resource arg [optional] string resource, TYPE:NAME:value
  --file-resource arg   [optional] file resource, TYPE:NAME:path

пример использования:

Windows:

patcher --executor-path="executor.exe" --icon-path="d:\tmp\icon.ico" --description=Installer --version=1.2.3 --product-name=Proceset --run-as-admin=true --string-resource=PARAM:CMD_LINE:"""<dir_path>\jre\bin\javaw.exe"" -cp ""<dir_path>\jar\*"" -Dlog_dir=""<dir_path>\logs"" com.infomaximum.installer.Main --work_dir ""<dir_path>"" --current_app_path ""<current_app_path>""" --string-resource=PARAM:WORKING_DIR:"<dir_path>\jre\bin" --file-resource=ZIP:DATA.ZIP:"d:\tmp\data.zip"

linux: (нужен wine x64)
wine '/home/rokunov/.wine/drive_c/patcher.exe' --executor-path="c:\executor.exe" --icon-path="c:\icon.ico" --description=Installer --version=1.2.3 --product-name=Proceset --run-as-admin=true --string-resource=PARAM:CMD_LINE:'"<dir_path>\jre\bin\javaw.exe" -cp "<dir_path>\jar\*" -Dlog_dir="<dir_path>\logs" com.infomaximum.installer.Main --work_dir "<dir_path>" --current_app_path "<current_app_path>"' --string-resource=PARAM:WORKING_DIR:"<dir_path>\jre\bin" --file-resource=ZIP:DATA.ZIP:"c:\data.zip"