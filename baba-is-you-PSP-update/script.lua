-- ============================================
-- Configurações iniciais
-- ============================================
bgcolor = Color.new(8,8,8)
tileSize = 24
frame = 0
playeranimation1 = 0
playerfacing = 0

-- Movement tuning: lower `moveLerp` = faster smoothing
moveLerp = 3
-- Arrival threshold in pixels (used to allow next move)
arrivalThreshold = 0.25

playerx = (tileSize*4)+4
playery = (tileSize*4)+4
playerxtarget = playerx
playerytarget = playery

objects = {}
rules = {}
rulesDirty = true

level = 1

-- Undo & Reset State Variables
historyStack = {}
resetConfirm = false

-- ============================================
-- Sons
-- ============================================

Wav.load("044.wav", 1)
Wav.load("music.wav", 0)
Wav.load("021.wav", 2)
Wav.play(true,0)

-- ============================================
-- Funções auxiliares
-- ============================================
function spawnObject(x, y, type, sprite)
    local obj = {
        x = x,
        y = y,
        xtarget = x,
        ytarget = y,
        type = type,
        sprite = sprite,
        properties = {},
        isText = (string.sub(type,1,5) == "TEXT_")
    }
    table.insert(objects, obj)
    return obj
end

-- Retorna TODOS os objetos numa determinada coordenada (suporta sobreposição)
function getObjectsAt(x, y)
    local found = {}
    for _,obj in ipairs(objects) do
        if math.floor(obj.xtarget) == x and math.floor(obj.ytarget) == y then
            table.insert(found, obj)
        end
    end
    return found
end

function tableContains(tbl, val)
    for _,v in ipairs(tbl) do
        if v == val then return true end
    end
    return false
end

-- ============================================
-- Sistema de Undo (Histórico)
-- ============================================
function saveState()
    local snapshot = {
        playerfacing = playerfacing,
        playeranimation1 = playeranimation1,
        objects = {}
    }
    for _, obj in ipairs(objects) do
        table.insert(snapshot.objects, {
            obj = obj,
            x = obj.x,
            y = obj.y,
            xtarget = obj.xtarget,
            ytarget = obj.ytarget
        })
    end
    table.insert(historyStack, snapshot)
end

function undo()
    if #historyStack == 0 then return end

    local snapshot = table.remove(historyStack)
    playerfacing = snapshot.playerfacing
    playeranimation1 = snapshot.playeranimation1

    for _, state in ipairs(snapshot.objects) do
        state.obj.x = state.x
        state.obj.y = state.y
        state.obj.xtarget = state.xtarget
        state.obj.ytarget = state.ytarget
    end

    rulesDirty = true
end

function loadLevel(filename)
    objects = {}  -- limpa objetos
    historyStack = {} -- limpa histórico de undo ao carregar fase

    local file = io.open(filename, "r")
    if not file then
        print("Erro ao carregar fase:", filename)
        return
    end

    local y = 0
    for line in file:lines() do
        for x = 1, #line do
            local char = line:sub(x, x)

            if char ~= "." then
                local def = charToObject[char]
                if def then
                    spawnObject(x - 1, y, def.type, def.sprite)
                end
            end
        end
        y = y + 1
    end

    file:close()
end


-- ============================================
-- Atualizar regras (Horizontal e Vertical)
-- ============================================
function updateRules()
    rules = {}

    for _,obj in ipairs(objects) do
        if obj.isText then
            -- Mapeamento Horizontal (Esq -> Dir)
            local h1_list = getObjectsAt(obj.xtarget + 1, obj.ytarget)
            local h2_list = getObjectsAt(obj.xtarget + 2, obj.ytarget)

            for _, r1 in ipairs(h1_list) do
                if r1.type == "TEXT_IS" then
                    for _, r2 in ipairs(h2_list) do
                        if r2.isText then
                            local subject = string.sub(obj.type, 6)
                            local property = string.sub(r2.type, 6)

                            rules[subject] = rules[subject] or {}
                            if not tableContains(rules[subject], property) then
                                table.insert(rules[subject], property)
                            end
                        end
                    end
                end
            end

            -- Mapeamento Vertical (Cima -> Baixo)
            local v1_list = getObjectsAt(obj.xtarget, obj.ytarget + 1)
            local v2_list = getObjectsAt(obj.xtarget, obj.ytarget + 2)

            for _, r1 in ipairs(v1_list) do
                if r1.type == "TEXT_IS" then
                    for _, r2 in ipairs(v2_list) do
                        if r2.isText then
                            local subject = string.sub(obj.type, 6)
                            local property = string.sub(r2.type, 6)

                            rules[subject] = rules[subject] or {}
                            if not tableContains(rules[subject], property) then
                                table.insert(rules[subject], property)
                            end
                        end
                    end
                end
            end

        end
    end
end

-- ============================================
-- Movimento / empurrão com suporte a sobreposição
-- ============================================
function canMoveTo(x, y, dx, dy)
    local chain = {}
    local cx, cy = x, y

    while true do
        local current_objs = getObjectsAt(cx, cy)
        if #current_objs == 0 then break end

        local hasPushable = false

        for _, obj in ipairs(current_objs) do
            -- STOP em qualquer objeto bloqueia a célula inteira
            if not obj.isText and
               rules[obj.type] and
               tableContains(rules[obj.type], "STOP") then
                return false
            end

            -- Adiciona objetos empurráveis (textos ou regras PUSH)
            if obj.isText or (rules[obj.type] and tableContains(rules[obj.type], "PUSH")) then
                table.insert(chain, obj)
                hasPushable = true
            end
        end

        -- Se nenhum objeto no tile era empurrável, interrompe a corrente
        if not hasPushable then
            break
        end

        cx = cx + dx
        cy = cy + dy
    end

    -- Move todos os objetos na corrente
    for _, obj in ipairs(chain) do
        obj.xtarget = obj.xtarget + dx
        obj.ytarget = obj.ytarget + dy
        if obj.isText then rulesDirty = true end
    end

    return true
end


-- ============================================
-- Checar vitória (suporta múltiplos objetos no mesmo tile)
-- ============================================
function checkWin()
    for _, obj in ipairs(objects) do
        if rules[obj.type] and tableContains(rules[obj.type], "YOU") then
            local target_objs = getObjectsAt(math.floor(obj.xtarget), math.floor(obj.ytarget))
            
            for _, targetObj in ipairs(target_objs) do
                if targetObj ~= obj and rules[targetObj.type] and tableContains(rules[targetObj.type], "WIN") then
                    level = level + 1
                    loadLevel("level" .. level .. ".txt")
                    rulesDirty = true
                    Wav.play(false, 2)
                    return
                end
            end
        end
    end
end

-- ============================================
-- Sprites do jogador (BABA)
-- ============================================
babaright0 = Image.load("babaright_0.png")
babaright1 = Image.load("babaright_1.png")
babaright2 = Image.load("babaright_2.png")
babaleft0 = Image.load("babaleft_0.png")
babaleft1 = Image.load("babaleft_1.png")
babaleft2 = Image.load("babaleft_2.png")
babaup0 = Image.load("babaup_0.png")
babaup1 = Image.load("babaup_1.png")
babaup2 = Image.load("babaup_2.png")
babadown0 = Image.load("babadown_0.png")
babadown1 = Image.load("babadown_1.png")
babadown2 = Image.load("babadown_2.png")

-- ============================================
-- Sprites dos objetos
-- ============================================
wallSprite = Image.load("wallobj.png")
rockSprite = Image.load("rock.png")
flagSprite = Image.load("flag.png")
tileSprite = Image.load("tile.png")

textWallSprite = Image.load("text_wall.png")
textIsSprite = Image.load("text_is.png")
textStopSprite = Image.load("text_stop.png")
textRockSprite = Image.load("text_rock.png")
textFlagSprite = Image.load("text_flag.png")
textPushSprite = Image.load("text_push.png")
textWinSprite = Image.load("text_win.png")
Empty = Image.load("empty.png")

textBabaSprite = Image.load("text_baba.png")
textYouSprite  = Image.load("text_you.png")

-- ============================================
-- Mapeamento de caracteres do level
-- ============================================
charToObject = {
    W = { type = "WALL", sprite = wallSprite },
    R = { type = "ROCK", sprite = rockSprite },
    F = { type = "FLAG", sprite = flagSprite },
    B = { type = "BABA", sprite = babadown0 },
    E = { type = "TILE", sprite = tileSprite },

    T = { type = "TEXT_WALL", sprite = textWallSprite },
    G = { type = "TEXT_ROCK", sprite = textRockSprite },
    I = { type = "TEXT_IS",   sprite = textIsSprite },
    Q = { type = "TEXT_FLAG", sprite = textFlagSprite },
    S = { type = "TEXT_STOP", sprite = textStopSprite },
    P = { type = "TEXT_PUSH", sprite = textPushSprite },
    V = { type = "TEXT_WIN",  sprite = textWinSprite },

    A = { type = "TEXT_BABA", sprite = textBabaSprite },
    Y = { type = "TEXT_YOU",  sprite = textYouSprite }
}

loadLevel("level" .. level .. ".txt")

-- ============================================
-- Loop principal
-- ============================================
while true do
    screen:clear(bgcolor)
    frame = frame + 1

    if rulesDirty then
        updateRules()
        rulesDirty = false
    end

    pad = Controls.read()

    -- 1. Botão SELECT para sair do loop
    if pad:select() then
        break
    end

    -- 2. Tratamento da Confirmação de Reset (R)
    if resetConfirm then
        screen:print(10, 10, "RESET LEVEL? Press X to confirm, O to cancel", Color.new(255, 255, 255))
        
        if pad:cross() then
            loadLevel("level" .. level .. ".txt")
            rulesDirty = true
            resetConfirm = false
        elseif pad:circle() then
            resetConfirm = false
        end
    else
        -- Ativar janela de confirmação de reset
        if pad:r() then
            resetConfirm = true
        end

        -- Execute Undo (L)
        if pad:l() then
            undo()
        end

        -- Processamento de movimento
        local dx, dy = 0, 0
        if pad:right() then dx = 1
        elseif pad:left() then dx = -1
        elseif pad:up() then dy = -1
        elseif pad:down() then dy = 1 end

        -- Execute movement when a direction is pressed
        if dx ~= 0 or dy ~= 0 then
            local movedAny = false

            -- Check if movement can happen BEFORE saving state
            local canMoveAny = false
            for _, obj in ipairs(objects) do
                if rules[obj.type] and tableContains(rules[obj.type], "YOU") then
                    if math.abs(obj.x - obj.xtarget) < arrivalThreshold and 
                       math.abs(obj.y - obj.ytarget) < arrivalThreshold then
                        local tx = math.floor(obj.xtarget) + dx
                        local ty = math.floor(obj.ytarget) + dy
                        if canMoveTo(tx, ty, dx, dy) then
                            canMoveAny = true
                            break
                        end
                    end
                end
            end

            -- Salva o estado atual no histórico ANTES de aplicar o movimento
            if canMoveAny then
                saveState()

                for _, obj in ipairs(objects) do
                    if rules[obj.type] and tableContains(rules[obj.type], "YOU") then
                        if math.abs(obj.x - obj.xtarget) < arrivalThreshold and 
                           math.abs(obj.y - obj.ytarget) < arrivalThreshold then
                            
                            local tx = math.floor(obj.xtarget) + dx
                            local ty = math.floor(obj.ytarget) + dy

                            if canMoveTo(tx, ty, dx, dy) then
                                obj.xtarget = tx
                                obj.ytarget = ty
                                movedAny = true
                            end
                        end
                    end
                end

                if movedAny then
                    playerfacing = (dx == 1 and 0) or (dx == -1 and 2) or (dy == -1 and 1) or 3
                    playeranimation1 = (playeranimation1 + 1) % 3
                    Wav.play(false, 1)
                end
            end
        end
    end

    -- ============================================
    -- Rendering Loop Adjustments
    -- ============================================

    -- 1. Draw Cosmetic Floor Tiles First (Background Layer)
    for _, obj in ipairs(objects) do
        if obj.type == "TILE" then
            obj.x = obj.x + (obj.xtarget - obj.x) / moveLerp
            obj.y = obj.y + (obj.ytarget - obj.y) / moveLerp
            screen:blit(obj.x * tileSize, obj.y * tileSize, obj.sprite)
        end
    end

    -- 2. Draw Interactive Base Sprites
    for _, obj in ipairs(objects) do
        if obj.type ~= "TILE" then
            obj.x = obj.x + (obj.xtarget - obj.x) / moveLerp
            obj.y = obj.y + (obj.ytarget - obj.y) / moveLerp

            if obj.type == "BABA" and rules["BABA"] and tableContains(rules["BABA"], "YOU") then
                -- Skip drawing base sprite for active Baba overlay
            else
                screen:blit(obj.x * tileSize, obj.y * tileSize, obj.sprite)
            end
        end
    end

    -- Render directional/animated overlays for YOU objects
    for _, obj in ipairs(objects) do
        if rules[obj.type] and tableContains(rules[obj.type], "YOU") then
            if obj.type == "BABA" then
                local px = (obj.x * tileSize) - 4
                local py = (obj.y * tileSize) - 4
                local anim = playeranimation1
                
                if playerfacing == 0 then screen:blit(px, py, ({babaright0, babaright1, babaright2})[anim + 1])
                elseif playerfacing == 2 then screen:blit(px, py, ({babaleft0, babaleft1, babaleft2})[anim + 1])
                elseif playerfacing == 1 then screen:blit(px, py, ({babaup0, babaup1, babaup2})[anim + 1])
                else screen:blit(px, py, ({babadown0, babadown1, babadown2})[anim + 1]) end
            end
        end
    end

    checkWin()

    screen.waitVblankStart()
    screen.flip()
end