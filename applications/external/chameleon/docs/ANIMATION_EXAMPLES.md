# 🎬 Exemplos de Uso das Animações

## Como Usar as Animações Contextuais

### Exemplo 1: Conexão USB com Animação de Cumprimento

```c
// Na cena de conexão USB
void chameleon_scene_usb_connect_on_enter(void* context) {
    ChameleonApp* app = context;

    // Tentar conectar via USB
    if(chameleon_app_connect_usb(app)) {
        // Sucesso! Mostrar animação de cumprimento
        chameleon_animation_view_set_type(app->animation_view, ChameleonAnimationHandshake);
        chameleon_animation_view_set_callback(app->animation_view, connection_success_callback, app);
        
        view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewAnimation);
        chameleon_animation_view_start(app->animation_view);
    } else {
        // Falha! Mostrar animação de erro
        chameleon_animation_view_set_type(app->animation_view, ChameleonAnimationError);
        chameleon_animation_view_start(app->animation_view);
    }
}
```

### Exemplo 2: Busca Bluetooth com Radar

```c
// Na cena de busca BLE
void chameleon_scene_ble_scan_on_enter(void* context) {
    ChameleonApp* app = context;

    // Mostrar animação de radar/busca
    chameleon_animation_view_set_type(app->animation_view, ChameleonAnimationScan);
    chameleon_animation_view_set_callback(app->animation_view, scan_complete_callback, app);
    
    view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewAnimation);
    chameleon_animation_view_start(app->animation_view);
    
    // Iniciar busca em paralelo
    chameleon_app_connect_ble(app);
}

static void scan_complete_callback(void* context) {
    ChameleonApp* app = context;
    
    // Verificar resultados da busca
    size_t device_count = ble_handler_get_device_count(app->ble_handler);
    
    if(device_count > 0) {
        // Encontrou dispositivos - ir para próxima cena
        scene_manager_next_scene(app->scene_manager, ChameleonSceneBleConnect);
    } else {
        // Não encontrou - mostrar erro
        chameleon_animation_view_set_type(app->animation_view, ChameleonAnimationError);
        chameleon_animation_view_start(app->animation_view);
    }
}
```

### Exemplo 3: Transferência de Dados

```c
// Durante operação de leitura/escrita
void chameleon_scene_tag_operation_on_enter(void* context) {
    ChameleonApp* app = context;

    // Mostrar animação de transferência
    chameleon_animation_view_set_type(app->animation_view, ChameleonAnimationTransfer);
    chameleon_animation_view_set_callback(app->animation_view, transfer_complete_callback, app);
    
    view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewAnimation);
    chameleon_animation_view_start(app->animation_view);
    
    // Iniciar operação de dados
    start_data_operation(app);
}

static void transfer_complete_callback(void* context) {
    ChameleonApp* app = context;
    
    // Mostrar comemoração de sucesso
    chameleon_animation_view_set_type(app->animation_view, ChameleonAnimationSuccess);
    chameleon_animation_view_start(app->animation_view);
}
```

### Exemplo 4: Configuração/Trabalho Técnico

```c
// Durante configuração de slots
void chameleon_scene_slot_config_on_enter(void* context) {
    ChameleonApp* app = context;

    // Mostrar animação de workshop/trabalho
    chameleon_animation_view_set_type(app->animation_view, ChameleonAnimationWorkshop);
    chameleon_animation_view_set_callback(app->animation_view, config_complete_callback, app);
    
    view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewAnimation);
    chameleon_animation_view_start(app->animation_view);
    
    // Aplicar configurações
    apply_slot_configuration(app);
}
```

## 🎨 Personalizações Avançadas

### Criando Nova Animação Personalizada

```c
// 1. Adicionar enum (em chameleon_animation_view.h)
typedef enum {
    // ... outras animações
    ChameleonAnimationCustom,
} ChameleonAnimationType;

// 2. Criar função de desenho (em chameleon_animation_view.c)
static void draw_custom_scene(Canvas* canvas, uint8_t frame) {
    // Limpar tela
    canvas_clear(canvas);
    
    // Desenhar golfinho com movimento personalizado
    int dolphin_x = 20 + (frame % 8) - 4; // Movimento horizontal
    draw_dolphin(canvas, dolphin_x, 35, frame);
    
    // Desenhar camaleão com animação única
    draw_chameleon(canvas, 80, 35, frame);
    
    // Adicionar elementos únicos
    if(frame % 4 < 2) {
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "Customizado!");
    }
    
    // Efeitos especiais
    for(int i = 0; i < frame % 6; i++) {
        canvas_draw_dot(canvas, 30 + i * 15, 50);
    }
}

// 3. Adicionar no switch da função draw_callback
case ChameleonAnimationCustom:
    draw_custom_scene(canvas, m->frame);
    break;
```

### Modificando Durações

```c
// Para animação mais longa
static void chameleon_animation_view_timer_callback(void* context) {
    // ... código existente ...
    
    // Parar após 64 frames (8 segundos) em vez de 32
    if(model->frame >= 64) {
        model->running = false;
        // ...
    }
}

// Para animação mais rápida
ChameleonAnimationView* chameleon_animation_view_alloc() {
    // ... código existente ...
    
    // Timer mais rápido: 12 FPS em vez de 8
    #define CUSTOM_FPS 12
    furi_timer_start(animation_view->timer, 1000 / CUSTOM_FPS);
}
```

## 🔧 Dicas de Implementação

### 1. Callback Inteligente
```c
static void smart_animation_callback(void* context) {
    ChameleonApp* app = context;
    
    // Verificar estado antes de decidir próxima ação
    if(app->connection_status == ChameleonStatusConnected) {
        scene_manager_next_scene(app->scene_manager, ChameleonSceneMainMenu);
    } else {
        scene_manager_previous_scene(app->scene_manager);
    }
}
```

### 2. Sequência de Animações
```c
static void animation_sequence_callback(void* context) {
    ChameleonApp* app = context;
    static uint8_t sequence_step = 0;
    
    switch(sequence_step) {
        case 0:
            // Primeira animação concluída, iniciar segunda
            chameleon_animation_view_set_type(app->animation_view, ChameleonAnimationWorkshop);
            chameleon_animation_view_start(app->animation_view);
            sequence_step++;
            break;
            
        case 1:
            // Segunda animação concluída, iniciar terceira
            chameleon_animation_view_set_type(app->animation_view, ChameleonAnimationSuccess);
            chameleon_animation_view_start(app->animation_view);
            sequence_step++;
            break;
            
        default:
            // Sequência completa
            sequence_step = 0;
            scene_manager_next_scene(app->scene_manager, ChameleonSceneMainMenu);
            break;
    }
}
```

### 3. Animação Condicional
```c
void choose_animation_by_context(ChameleonApp* app, ChameleonConnectionType connection_type) {
    ChameleonAnimationType animation_type;
    
    switch(connection_type) {
        case ChameleonConnectionUSB:
            animation_type = ChameleonAnimationHandshake; // Formal para USB
            break;
            
        case ChameleonConnectionBLE:
            animation_type = ChameleonAnimationBar; // Casual para Bluetooth
            break;
            
        default:
            animation_type = ChameleonAnimationError;
            break;
    }
    
    chameleon_animation_view_set_type(app->animation_view, animation_type);
    chameleon_animation_view_start(app->animation_view);
}
```

## 📊 Performance e Otimização

### Usando Animações Eficientemente

```c
// ✅ BOM: Parar animação anterior antes de iniciar nova
void switch_animation_safely(ChameleonApp* app, ChameleonAnimationType new_type) {
    chameleon_animation_view_stop(app->animation_view); // Parar primeiro
    chameleon_animation_view_set_type(app->animation_view, new_type);
    chameleon_animation_view_start(app->animation_view);
}

// ❌ EVITAR: Múltiplas animações simultâneas
// Não fazer isso - pode causar problemas de performance
```

### Gerenciamento de Memória

```c
// ✅ BOM: Cleanup adequado
void chameleon_scene_on_exit(void* context) {
    ChameleonApp* app = context;
    
    // Sempre parar animações ao sair da cena
    chameleon_animation_view_stop(app->animation_view);
    
    // Resetar outros elementos se necessário
    popup_reset(app->popup);
}
```

---

**Com essas animações contextuais, cada interação do usuário com o Chameleon Ultra se torna uma experiência visual única e divertida!** 🎭✨