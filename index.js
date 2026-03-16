function startChat() {

    const usernameInput = document.getElementById("nameInput");
    const username = usernameInput.value.trim();

    if (username === "") {
        alert("Please enter your name");
        return;
    }

    localStorage.setItem("username", username);

    window.location.href = "chat.html";
}