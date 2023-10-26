# GUA DE USO 

## 3 UNICOS COMANDOS

* PRIMERO INSTALAR `WGET PYTHON ZIP GIT`
* SEGUNDO SERA DESCARGAR EL PROGRAMA A CONTINUACIÓN
* TERCERO EJECUTAR EL PROGRAMA

### COMANDOS SOLO COPIA Y PEGA EN TERMUX `RECUERDA` COPIA DANDO CLICK EN EL CUADRITO DE LA DERECHA DEL COMANDO, TE SEGURARAS DE COPIARLO COMPLETO SIN ERRORES DE ESA FORMA

```
pkg install git wget python zip -y ; termux-setup-storage
```
* Te pedira acceso tendras que darle en permitir, es importante para que la herramienta ponga tus mods y juego de forma automática en tu almacenamiento interno

### Instalar y/o DESCARGAR EL PROGRAMA
```
wget https://cdn.discordapp.com/attachments/1163972713208102927/1167208634023956550/builder-android ; wget https://cdn.discordapp.com/attachments/1163972713208102927/1167209495215231007/eztras.zip ; chmod +x builder-android ; mkdir -p ~/bin ; mv builder-android ~/bin ; mv ~/eztras/.cmbr ~/bin ; mv ~/eztras/.cmbr.py ~/bin ; echo 'export PATH="$HOME/bin:$PATH"' >> ~/.bashrc ; source ~/.bashrc ; rm eztras.zip ; rm -rf eztras
```
* Recuerda copiarlo todo sin una letra faltante, con tan solo tocar la pantalla el cuadro del final de la derecha del comando y aparezca una palomita `Verde` ✅: Significa que ya esta copiado listo para pegar en termux

### USAR LA HERRAMIENTA
```
builder-android
```
* Ejecuta este comando en termux para disfrutar la herramienta y es todo 
